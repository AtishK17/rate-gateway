#pragma once

#include <drogon/nosql/RedisClient.h>
#include <drogon/orm/DbClient.h>
#include "../limiers/RateLimiter.h"
#include "../limiers/TokenBucketLimiter.h"
#include "../limiers/SlidingWindowLimiter.h"
#include "ApiKeyManager.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <atomic>

struct GatewayStats {
	int total_keys;
	long requests_1m;
	long throttled_1m;
	double avg_latency_ms;
};

class RuleStore {
public:
	RuleStore(
		drogon::nosql::RedisClientPtr redis,
		drogon::orm::DbClientPtr db
	);
	
	void loadFromDb(const std::vector<ApiKeyRecord>& keys);
	
	std::shared_ptr<RateLimiter> getLmiter(const std::string& key) const;
	
	void registerKey(const std::string& key);
	
	GatewayStats getAggregateStats() const;
	
	void recordRequest(bool allowed, double latency_ms);
	
private:
	drogon::nosql::RedisClientPtr redis_;
	drogon::orm::DbClientPtr db_;
	
	mutable std::shared_mutex mutex_;
	std::unordered_map<std::string, std::shared_ptr<RateLimiter>> limiters_;
	
	std::atomic<long> totalRequests_{0};
	std::atomic<long> throttledRequests_{0};
	std::atomic<double> totalLatencyMs_{0.0};
	
	std::shared_ptr<RateLimiter> buildLimiter(const ApiKeyRecord& record) const;
};
