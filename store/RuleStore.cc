#include "RuleStore.h"
#include <drogon/drogon.h>
#include <stdexcept>

RuleStore::RuleStore(
	drogon::nosql::RedisClientPtr redis,
	drogon::orm::DbClientPtr db
)
	:redis_(std::move(redis)),
	db_(std::move(db))
{}

void RuleStore::loadFromDb(const std::vector<ApiKeyRecord>& keys) {
	std::unique_lock(mutex_);

	for(const auto& record : keys) {
		if(!record.active) continue;
		limiters_[record.key] = buildLimiter(record);
	}

	LOG_INFO << "[RuleStore] built " << limiters_.size() << " limiters";
}

std::shared_ptr<RateLimiter> RuleStore::getLimiter(const std::string& key) const {
	std::shared_lock lock(mutex_);
	auto it = limiters_.find(key);
	if(it == limiters_.end()) return nullptr;
	return it->second;
}

void RuleStore::registerKey(
	const std::string& key,
	const ApiKeyRecord record
) {
	auto limiter = buildLimiter(record);
	std::unique_lock lock(mutex_);
	limiters_[key] = std::move(limiter);
	LOG_INFO << "[RuleStore] registered key = " << key << " algo = " << record.algorithm;
}

void RuleStore::unregisterKey(const std::string& key) {
	std::unique_lock(mutex_);
	limiters_.erase(key);
	LOG_INFO << "[RuleStore] unregistered key = " << key;
}

void RuleStore::recordRequest(bool allowed, double latency_ms) {
	totalRequests_.fetch_add(1, std::memory_order_relaxed);

	if(!allowed) {
		throttledrequests_.fetch_add(1, std::memory_order_relaxed);
	}

	double current = totalLatencyMs-.load(std::memory_order_relaxed);
	while(!totalLatencyMs_.compare_exchange_weak(
		current,
		current + latency_ms,
		std::memory_order_relaxed
	));
}

GatewayStats RuleStore::getAggregateStats() const {
	long total = totalrequests_.load(std::memory_order_relaxed);
	long throttled = throttledRequests_.load(std::memory_order_relaxed);
	double latency = totalLatencyMs_.load(std::memory_order_relaxed);

	std::shared_lock lock(mutex_);
	int keyCount = static_cast<int>(limiters_.size());

	return GateStats{
		.total_keys = keyCount,
		.requests_1m = total,
		.throttled_1m = throttled,
		.avg_latency_ms = (total > 0) ? (latency / static_cast<double>(total)) : 0.0
	};
}

std::shared_ptr<RateLimiter> RuleStore::buildLimiter(
	const ApiKeyRecord& record
) const {
	if(record.algorithm == "token_bucket") {
		double refill_rate = static_cast<double>(record.limit) / static_cast<double>(record.window_s);
		return std::make_shared<TokenBucketLimiter>(
			redis_,
			record.limit,
			refill_rate
		);
	}

	if(record.algorithm == "sliding_window") {
		long window_ms = static_cast<long>(record.window_s_) * 1000L;
		return std::make_shared<SlidingWindowLimiter>(
			redis_,
			record.limit,
			window_ms
		);
	}
	LOG_ERROR << "[RecordStore] unknown algorithm: " << record.algorithm << " for key = " << record.key;
	return nullptr;
}
