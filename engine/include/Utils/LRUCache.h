#pragma once

#include <functional>
#include <list>
#include <unordered_map>

namespace Utils {

template <typename Key, typename Value>
class LRUCache {
public:
	using EvictionCallback = std::function<void(const Key&, Value&)>;

	explicit LRUCache(size_t maxSize, EvictionCallback onEvict = nullptr)
		: maxSize(maxSize), onEvict(onEvict)	
	{}

	Value* get(const Key& key) {
		auto it = cacheMap.find(key);
		if (it == cacheMap.end())
			return nullptr;

		items.splice(items.begin(), items, it->second);
		return &(it->second->second);
	}

	void put(const Key& key, Value value) {
		auto it = cacheMap.find(key);

		if (it != cacheMap.end()) {
			it->second->second = std::move(value);
			items.splice(items.begin(), items, it->second);
			return;
		}

		if (items.size() >= maxSize)
			evictLRU();

		items.emplace_front(key, std::move(value));
		cacheMap[key] = items.begin();
	}

	bool contains(const Key& key) const {
		return cacheMap.find(key) != cacheMap.end();
	}

	void remove(const Key& key) {
		auto it = cacheMap.find(key);
		if (it != cacheMap.end()) {
			if (onEvict)
				onEvict(it->second->first, it->second->second);

			items.erase(it->second);
			cacheMap.erase(it);
		}
	}

	void clear() {
		if (onEvict) {
			for(auto& item : items)
				onEvict(item.first, item.second);
		}

		items.clear();
		cacheMap.clear();
	}

	size_t size() const {
		return items.size();
	}

	size_t capacity() const {
		return maxSize;
	}

	bool isFull() const {
		return items.size() >= maxSize;
	}

private:
	size_t maxSize;
	EvictionCallback onEvict = nullptr;

	std::list<std::pair<Key, Value>> items;
	std::unordered_map<Key, typename std::list<std::pair<Key, Value>>::iterator> cacheMap;

	void evictLRU() {
		if (items.empty())
			return;

		auto& lru = items.back();

		if (onEvict)
			onEvict(lru.first, lru.second);

		cacheMap.erase(lru.first);
		items.pop_back();
	}
};

} // namespace Utils
