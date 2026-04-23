#include "TokenBucketLimiter.h"
#include <chrono>
#include <future>
#include <stdexcept>
#include <vector>

// ---------------------------------------------------------------------------
// Lua script — runs atomically inside Redis (no race conditions possible)
//
// Steps:
//  1. Read current tokens + last_refill timestamp from the hash
//  2. Compute how many tokens to add based on elapsed time
//  3. Cap at capacity
//  4. If tokens >= 1: consume one, mark allowed
//  5. Save updated state back to Redis with a TTL
//  6. Return {allowed, remaining, retry_after_ms}
// ---------------------------------------------------------------------------

const std::string TokenBucketLimiter::kLuaScript = R"lua(
local key = KEYS[1]
local capacity = tonumber(ARGV[1])
local refill_per_ms = tonumber(ARGV[2])
local now = tonumber(ARGV[3])

-- Load existing bucket state (nil if first request)
local data = redis.call('HMGET', key, 'tokens', 'last_refill')
local tokens = tonumber(data[1]) or capacity
local last_refill = tonumber(data[2]) or now
 
-- Refill: add tokens proportional to elapsed time
local elapsed  = math.max(0, now - last_refill)
local refilled = math.min(capacity, tokens + elapsed * refill_per_ms)
 
-- Decide allow or deny
local allowed      = 0
local retry_after  = 0
 
if refilled >= 1 then
    refilled = refilled - 1
    allowed  = 1
else
    -- Time until we have 1 full token again
    retry_after = math.ceil((1 - refilled) / refill_per_ms)
end
 
-- Persist updated state
-- TTL = time to fully refill an empty bucket + 1s buffer
local ttl = math.ceil(capacity / refill_per_ms / 1000) + 1
redis.call('HMSET', key, 'tokens', refilled, 'last_refill', now)
redis.call('EXPIRE', key, ttl)
 
return {allowed, math.floor(refilled), retry_after}
)lua";

// constructor

TokenBucketLimiter::TokenBucketLimiter(
	drogon::nosql::RedisClientPtr redis,
	int capacity,
	double refill_rate
)
	: redis_(std::move(redis)),
	capacity_(capacity),
	refill_rate(refill_rate),
	refill_per_ms(refill_rate / 1000.0)
{}

// isAllowed
RateLimitResult TokenBucketLimiter::isAllowed(const std::string& key) {
	auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()
	).count();
	
	std::vector<std::string> args = {
		kLuaScript, // the script to run
		"1", //number of KEYS arguments
		bucketKey(key), // KEYS[1]
		std::to_string(capacity_), //ARGV[1]
		std::to_string(refill_per_ms), //ARGV[2]
		std::to_string(now_ms) //ARGV[3]
	};
	
	try {
		auto result = execSync("EVAL", args);
		
		auto arr = result.asArray();
		bool allowed = arr[0].asInteger() == 1;
		int remaining = static_cast<int>(arr[1].asInteger());
		long retry_ms = arr[2].asInteger();
		
		return RateLimitResult{
			.allowed = allowed,
			.remaining = remaining,
			.limit = capacity_,
			.retry_after_ms = retry_ms
		};
	}
	catch(const std::exception& e) {
		LOG_ERROR << "TokenBucketLimiter Redis error: " << e.what();
		return RateLimitResult{
			.allowed = true,
			.remaining = capacity_,
			.limit = capacity_,
			.retry_after_ms = 0
		};
	}
}

// getStatus
std::optional<RateLimitStatus> TokenBucketLimiter::getStatus(
	const std::string& key
) {
	try{
		auto result = execSync("HMGET", {bucketKey(key), "tokens", "last_refill"});
		auto arr = result.asArray();
		
		if(arr[0].isNil()) return std::nullopt;
		
		auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_scince_epoch()
		).count();
		
		double tokens = std::stod(arr[0].asString());
		long last_refill = std::stol(arr[1].asString());
		
		double elapsed = static_cast<double>(now_ms - last refill);
		double current = std::min(
			static_cast<double>(capacity_),
			tokens + elapsed * refill_per_ms_
		);
		
		return RateLimitStatus{
			.key = key,
			.token_remaining = static_cast<int>(current),
			.limit = capscity_,
			.window_ms = static_cast<long>(capacity_ / refill_per_ms),
			.last_seen = std::chrono::system_clock::now()
		};
	}
	catch() {
		return std::nullopt;
	}
}

// reset
void TokenBucketLimiter::reset(const std::string& key) {
	try {
		execSync("DEL", {bucketKey(key)});
	}
	catch(const std::exception& e) {
		LOG_ERROR << "TokenBucketLimiter reset error: " << e.what();
	}
}

// Helpers
std::string TokenBucketLimiter::bucketKey(const std::string& key) const {
	return "bucket:" + key;
}

// Bridges Drogon's async Redis callback into a blocking call.
// Uses std::promise/future — safe for short-lived Redis round-trips.
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
