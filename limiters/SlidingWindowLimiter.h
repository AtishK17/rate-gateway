#pragma once

#include "RateLimiter.h"
#include <drogon/nosql/RedisClient.h>
#include <memory>
#include <string>

class SlidingWindowLimiter : public RateLimiter {
public:
	SlidingWindowLimiter(
		drogon::nosql::RedisClientPtr redis,
		int max_requests,
		long window_ms
	);
	
	RateLimitResult isAllowed(const std::string& key) override;
	
	std::optional<RateLimitStatus> getStatus(std::string& key) override;
	
	void reset(std::string& key) override;
	
	std::string algorithmName() const override {return "sliding_window";}

private:
	drogon::nosql::RedisClientPtr redis_;
	int max_requests_;
	long window_ms_;
	
	std::string windowKey(const std::string& key) const;
	
	static const std::string kLuaScript;
	
	drogon::nosql::RedisResult execSync(
		const std::string& cmd,
		const std::vector<std::string>& args
	);
	
	std::string uniqueMember(long now_ms) const;
};
