#pragma once

#include <memory>
#include <unordered_map>

#include "Assets/IAssetStorage.h"

namespace Blackthorn::Assets {

template <typename AssetType>
class AssetStorage : public IAssetStorage {
private:
	std::unordered_map<std::string, std::unique_ptr<AssetType>> assets;

public:
	std::unique_ptr<AssetType>& get(const std::string& id) {
		return assets[id];
	}

	bool has(const std::string& id) const override {
		return assets.find(id) != assets.end();
	}

	void add(const std::string& id, std::unique_ptr<AssetType> asset) {
		assets[id] = std::move(asset);
	}

	void remove(const std::string& id) override {
		assets.erase(id);
	}

	void clear() override {
		assets.clear();
	}

	size_t size() const override {
		return assets.size();
	}

	size_t getMemoryUsage() const override {
		size_t total = 0;

		for (const auto& [id, asset] : assets) {
			if constexpr (requires { asset->getMemoryUsage(); }) {
				total += asset->getMemoryUsage();
			} else {
				total += sizeof(AssetType);
			}
		}

		return total;
	}

	std::vector<std::string> getAllIDs() const override {
		std::vector<std::string> ids;

		ids.reserve(assets.size());

		for (const auto& [id, _] : assets)
			ids.push_back(id);

		return ids;
	}

	auto begin() { return assets.begin(); }
	auto begin() const { return assets.begin(); }
	auto end() { return assets.end(); }
	auto end() const { return assets.end(); }
};

} // namespace Blackthorn::Assets