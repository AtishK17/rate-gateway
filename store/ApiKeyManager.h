#pragma once

#include <drogon/orm/DbClient.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <optional>

struct ApiKeyRecord {
	std::string key;
	std::string name;
	std::string algorithm;
	int limit;
	int window_s;
	bool active;
};

class ApiKeyManager {
public:
	explicit ApiKeyManager(drogon::orm::DbClientPtr db);
	
	void loadFromDb();
	
	bool isValid(const std::string& key) const;
	
	std::optional<ApiKeyRecord> getRecord(const std::string& key) const;
	
	std::vector<ApiKeyRecord> getAllKeys() const;
	
	ApiKeyRecord createKey(const ApiKeyRecord& record);
	
	void deleteKey(const std::string& key);
	
private:
	drogon::DbClientPtr db_;
	
	mutable std::shared_mutex mutex_;
	std::unordered_map<std::string, ApiKeyRecord> cache_;
	
	std::string generateKey() const;
};
	
