#pragma once

#include "RateLimiter.h"
#include <drogon/nosql/RedisClient.h>
#include <memory>
#include <string>

class TokenBucketLimiter : public RateLimiter {
	public:
		TokenBucketLimiter(
			drogon::nosql::RedisClientPtr redis,
			int capacity,
			double refill_rate
		);
		
		RateLimiteResult isAllowed(const std::string& key) override;
		
		std::optimal<RateLimitStatus> getStatus(const std::string& key) override;
		
		void reset(const std::string& key) override;
		
		std::string algorithmName() const ovverride {return "token_bucket";}
		
	private:
		drogon::nosql::RedisClientPtr redis_;
		int capacity_;
		double refill_rate_; // tokens per second
		double refill_per_ms_; // pre-computed: refill_rate_ / 1000.0
		
		// Redis key for a given API key
		std::string bucketKey(const std::string& key) const;
		
		static const std::string kLuaScript;
		
		drogon::nosql::RedisResult exexSync(
			const std::string& cmd,
			const std::vector<std::string>& args
		);
};	
