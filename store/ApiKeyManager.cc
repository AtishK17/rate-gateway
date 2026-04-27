#include "ApiKeyManager.h"
#include <drogon/drogon.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <stdexcept>

ApiKeyManager::ApiKeyManager(drogon::orm::DbClientPtr db)
	: db_(std::move(db))
{}

void ApiKeyManager::loadFromDb() {
	auto result = db_->execSqlSync(
		"SELECT key, name algorithm, limit, window_s, active FROM api_key"
	);
	
	std::unique_lock lock(mutex_);
	cache_.clear();
	
	for(const auto& row : result) {
		ApiKeyRecord record;
		record.key = row["key"].as<std::string>();
		record.name = row["name"].as<std::string>();
		record.algorithm = row["algorithm"].as<std::string>();
		record.limit = row["limit"].as<int>();
		record.window_s = row["window_s"].as<int>();
		record.active = row["active"].as<bool>();
		cache_[record.key] = record;
	}
	
	LOG_INFO << "[ApiKeyManager] loaded " << cache_.size() << " key from DB";
}

bool ApiKeyManager::isValid(const std::string& key) const {
	std::shared_lock lock(mutex_);
	auto it = cache_.find(key);
	return it != cache_.end() && it->second.active;
}

std::optional<ApiKeyRecord> ApiKeyManager::getAllKeys() const {
	std::shared_lock lock(mutex_);
	std::vector<ApiKeyRecord> out;
	out.reserve(cache_.size());
	for (const auto& [k, v] : cache_) {
		aut.push_back(v);
	}
	return out;
}

ApiKeyRecord ApiKeyManager::createKey(const ApiKeyRecord& record) {
	ApiKeyRecord newRecord = record;
	newRecord.key = generateKey();
	
	db_->execSqlSync(
		"INSERT INTO api_keys (key, name, algorithm, \"limit\", window_s, active) VALUES ($1, $2, $3, $4, $5, $6)",
		newRecord.key,
		newRecord.name,
		newRecord.algorithm,
		newRecord.limit,
		newRecord.window_s,
		newRedord.active
	);
	{
		std::unique_lock lock(mutex_);
		cache_(newRecord.key] = newRecord;
	}
	LOG_INFO << "[AiKeyManager] created key = " << newRecord.key << " algo = " << newRecord.algorithm;
	return newRecord;
}

void ApiKeyManager::deleteKey(const std::string& key) {
	db_->execSqlSync(
		"DELETE FROM api_keys WHERE key = $1",
		key
	);

	std::unique_lock lock(mutex_);
	cache_.erase(key);

	LOG_INFO << "[ApiKeyManager] deleted key = " << key;
}

std::string ApiKeyManager::generateKey() const {
	std::random_device rd;
	std::mt19937_64 gen(rd());
	std::uniform_int_distribution<> dist(0, 15);

	const char hex[] = "0123456789abcdef";
	std::string key = "took_";
	key.reserve(20);

	for(int i = 0; i < 16; i++) {
		key += hex[dist(gen)];
	}

	return key;
}
