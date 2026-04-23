#pragma once

#include <string>
#include <chrono>
#include <optional>

// Holds the result of an isAllowed() check.
// The filter uses this to build the HTTP response and headers.
struct RateLimitResult {
	bool allowed;
	int remaining;
	int limit;
	long retry_after_ms;
};

// Snapshot of a key's current state — used by the admin API
// to expose live stats without affecting the counter.
struct RateLimitStatus {
	std::string key;
	int tokens_remaining;
	int limit;
	long window_ms;
	std::chrono::system_clock::time_point last_seen;
};

class RateLimiter {
	public:
		virtual ~RateLimiter() = default;
		
		virtual RateLimitResult isAllowed(const std::string& key) = 0;
		
		virtual std::optional<RateLimitStatus> getStatus(const std::string& key) = 0;
		
		virtual void reset(const std::string& key) = 0;
		
		// Human-readable name of the algorithm — shown in API responses
    		// and logs so you know which limiter handled a request.
		virtual std::string algorithmName() const = 0;
};
