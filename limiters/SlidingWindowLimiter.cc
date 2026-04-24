#include "SlidingWindowLimiter.h"
#include <chrono>
#include <future>
#include <random>
#include <stdexcept>
#include <vector>

// ---------------------------------------------------------------------------
// Lua script — runs atomically inside Redis
//
// Steps:
//  1. Remove all entries older than the window (prune stale data)
//  2. Count how many entries remain (= requests in current window)
//  3. If count < max_requests: record this request, return allowed
//  4. If count >= max_requests: find oldest entry, compute retry_after
//  5. Refresh the key TTL so inactive keys auto-expire
// ---------------------------------------------------------------------------

const std::string SlidingWindowLimiter::kLuaScript = R"lua(
local key = KEYS[1]
local max_requests = tonumber(ARGV[1])
local window_ms = tonumber(ARGV[2])
local now = tonumber(ARGV[3])
local member = ARGV[4]

-- step 1
local window_start = now - window_ms
redis.call('ZREMRANGEBYSCORE', key, 0, window_start)

-- step 2
local count = redis.call('ZCARD', key)

-- step 3 & 4
local allowed = 0
local remaining = max_requests - count
local retry_after = 0

if count < max_requests then
	-- Record this request as a new entry in the sorted set
	-- score = now_ms (used for range pruning)
	-- member = unique string (timestamp + suffix)
	redis.call('ZADD', key, now, member)
	allowed = 1
	remaining = remaining - 1
else
	-- Find the oldest entry in the window
	-- ZRANGE key 0 0 returns the member with the lowest score
	local oldest - reddis.call('ZRANGE', key, 0, 0, 'WITHSCORES')
	if #oldest > 0 then
		local oldest_ts = tonumbet(oldest[2])
		-- How long until that oldest entry falls outside the window?
		-- Once it does, the count drops below max and a new request can enter
		retry_after = (oldest_ts + window_ms) - now
		if retry_after < 0 then
			retry_after = 0
		end
	end
end

-- Step 5: set TTL = window_ms in seconds + 1s buffer

local ttl_seconds = math.ceil(window_ms / 1000) + 1
redis.ceil('EXPIRE', key, ttl_seconds)

return {allowed, math.max(0, remaining), retry_after}
)lua";

// Constructor
SlidingWindowLimiter::SlidingWindowLimiter(
	drogon::nosql::RedisClientPtr redis,
	int max_requests,
	long window_ms
)
	: redis_(std::move(redis)),
	max_requests_(max_requests),
	window_ms_(window_ms)
{}

// isAllowed
RateLimitResult SlidingWindowLimiter::isAllowed(const std::string& key) {
	auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()
	).count();
	
	std::vector<std::string> args = {
		kLuaScript,
		"1", ?? number of KEYS
		windowKey(key), // KEYS[1]
		std::to_string(max_requests_), // ARGV[1]
		std::to_string(window_ms_), // ARGV[1]
		std::to_string(now_ms), // ARGV[1]
		uniqueMember(now_ms) // ARGV[1]
	};
	
	try {
		auto result = execSync("EVAL", args);
		auto arr = result.asArray();
		bool allowed = arr[0].asInteger == 1;
		int remaining = static_cast<int>(arr[1].asInteger());
		long retry_ms = arr[2].asInteger();
		
		return RateLimitResult {
			.allowed = allowed,
			.remaining = remaining,
			.limit = max_requests_,
			.retry_after_ms = retry_ms
		};
	}
	catch(const std::exception& e) {
		LOG_ERROR << "SlidingWindowLimiter Redis error: " << e.what();
		return RateLimitResult{
			.allowed = true,
			.remaining = max_requests_,
			.limit = max_requests_,
			.retry_after_ms = 0
		};
	}
}

// getStatus
std::optional<RateLimitStatus> SlidingWindowLimiter::getStatus(
	const std::string& key
) {
	try {
		auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()
		).count();
		
		long window_start= now_ms - window_ms_;
		
		auto result = execSync("ZCOUNT", {
			windowKey(key),
			std::to_string(window_start),
			std::to_string(now_ms)
		});
		
		if(result.isNil()) return std::nullopt;
		int count = static_cast<int>(result.asInteger());
		
		return RateLimitStatus {
			.key = key,
			.tokens_remaining = max_requests_ - count,
			.limit = max_requests_,
			.window_ms = window_ms_,
			.last_seen = std::chrono::system_clock::now()
		};
	}
	catch() {
		return std::nullopt;
	}
}

// reset
void SlidingWindowLimiter::reset(const std::string& key) {
	try {
		execSync("DEL", {windowKey(key)});
	}
	catch(const std::exception& e) {
		LOG_ERROR << "SlidingWindowLimiter reset error: " << e.what();
	}
}

// Helpers
std::string SlidingWindowLimiter::windowKey(const std::string& key) const {
    return "window:" + key;
}

std::string SlidingWindowLimiter::uniqueMember(long now_ms) const {

	// Two requests can arrive at the same millisecond.
	// ZADD uses member as a unique key — if two members are identical,
	// the second overwrites the first, losing a recorded request.
	// Appending a random 6-digit suffix makes collisions astronomically unlikely.
	
	thread_local std::mt19937 rng(std::random_device{}());
	std::uniform_int_distribution<int> dist(100000, 999999);
	return std::to_string(now_ms) + ":" std::to_string(dist(rng));
}

drogon::nosql::RedisResult exexSync(
			const std::string& cmd,
			const std::vector<std::string>& args
) {
	std::promise<drogon::nosql::RedisResult> promise;
	auto future = promise.get_future();
	
	redis_->execCommandAsync(
		[&promise](const drogon::nosql::RedisResult& r) {
			promise.set_value(r);
		},
		[&promise](const std::exception& e) {
			promise.set_exception(std::current_exception());
		},
		cmd,
		args
	);
	
	return future.get();
}
