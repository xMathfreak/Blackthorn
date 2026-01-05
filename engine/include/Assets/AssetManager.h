#pragma once

#include <filesystem>
#include <string>
#include <typeindex>
#include <unordered_map>

#include "Core/Export.h"
#include "Assets/AssetHandle.h"
#include "Assets/AssetStorage.h"
#include "Assets/IAssetLoader.h"
#include "Assets/IAssetStorage.h"

namespace Blackthorn::Assets {

class BLACKTHORN_API AssetManager {
public:
	AssetManager() = default;
	~AssetManager() = default;

	AssetManager(const AssetManager&) = delete;
	AssetManager& operator=(const AssetManager&) = delete;

	template<typename AssetType>
	void registerLoader(std::unique_ptr<IAssetLoader<AssetType>> loader) {
		std::type_index type = std::type_index(typeid(AssetType));
		loaders[type] = std::make_unique<LoaderWrapper<AssetType>>(std::move(loader));

		if (storages.find(type) == storages.end())
			storages[type] = std::make_unique<AssetStorage<AssetType>>();
	}

	template <typename AssetType>
	AssetType* load(const std::string& id, const LoadParams& params) {
		std::type_index type = std::type_index(typeid(AssetType));

		auto it = loaders.find(type);
		if (it == loaders.end())
			return nullptr;

		if (!it->second->load(*this, id, params))
			return nullptr;

		assetParams[id] = params.clone();
		return get<AssetType>(id);
	}

	template <typename AssetType>
	AssetType* load(const std::string& id, const std::string& path) {
		PathLoadParams params(path);
		return load<AssetType>(id, params);
	}

	template <typename AssetType>
	AssetType* load(const std::string& path) {
		std::filesystem::path p(path);
		std::string id = p.stem().string();
		return load<AssetType>(id, path);
	}

	template <typename AssetType>
	size_t loadDirectory(const std::string& directory, bool recursive = false) {
		std::type_index type = std::type_index(typeid(AssetType));

		auto loaderIt = loaders.find(type);
		if (loaderIt == loaders.end())
			return 0;

		auto* lw = static_cast<LoaderWrapper<AssetType>*>(loaderIt->second.get());
		auto exts = lw->loader->getSupportedExtensions();

		size_t loaded = 0;

		if (recursive) {
			for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
				if (!entry.is_regular_file())
					continue;

				std::string ext = entry.path().extension().string();
				if (std::find(exts.begin(), exts.end(), ext) != exts.end()) {
					if (load<AssetType>(entry.path().string()))
						++loaded;
				}
			}
		} else {
			for (const auto& entry : std::filesystem::directory_iterator(directory)) {
				if (!entry.is_regular_file())
					continue;

				std::string ext = entry.path().extension().string();
				if (std::find(exts.begin(), exts.end(), ext) != exts.end()) {
					if (load<AssetType>(entry.path().string()))
						++loaded;
				}
			}
		}

		return loaded;
	}

	template <typename AssetType>
	void add(const std::string& id, std::unique_ptr<AssetType> asset) {
		getStorage<AssetType>()->add(id, std::move(asset));
	}

	template <typename AssetType>
	void alias(const std::string& existingID, const std::string& newID) {
		if (!has<AssetType>(existingID))
			return;

		aliases[newID] = existingID;
	}

	template <typename AssetType>
	AssetType* get(const std::string& id) {
		if (aliases.find(id) != aliases.end())
			return get<AssetType>(aliases[id]);

		auto storage = getStorage<AssetType>();
		if (!storage || !storage->has(id))
			return nullptr;

		return storage->get(id).get();
	}

	template <typename AssetType>
	const AssetType* get(const std::string& id) const {
		return const_cast<AssetManager*>(this)->get<AssetType>(id);
	}

	template <typename AssetType>
	AssetHandle<AssetType> getHandle(const std::string& id) {
		return AssetHandle<AssetType>(id, this);
	}

	template <typename AssetType>
	bool has(const std::string& id) const {
		if (aliases.find(id) != aliases.end())
			return has<AssetType>(aliases.at(id));

		auto* storage = getStorage<AssetType>();
		return storage && storage->has(id);
	}

	template <typename AssetType>
	void unload(const std::string& id) {
		auto storage = getStorage<AssetType>();

		if (storage) {
			storage->remove(id);
			assetParams.erase(id);
		}
	}

	template <typename AssetType>
	void unloadAll() {
		auto* storage = getStorage<AssetType>();
		if (storage) {
			for (const auto& id : storage->getAllIDs())
				assetParams.erase(id);

			storage->clear();
		}
	}

	void clear() {
		for (auto& [type, storage] : storages)
			storage->clear();

		assetParams.clear();
		aliases.clear();
	}

	template <typename AssetType>
	bool reload(const std::string& id) {
		auto pathIt = assetParams.find(id);
		if (pathIt == assetParams.end())
			return false;

		auto paramCopy = pathIt->second->clone();
		unload<AssetType>(id);

		return load<AssetType>(id, *paramCopy) != nullptr;
	}

	template <typename AssetType>
	size_t reloadAll() {
		auto* storage = getStorage<AssetType>();

		if (!storage)
			return 0;

		struct ReloadItem {
			std::string id;
			std::unique_ptr<LoadParams> param;
		};

		std::vector<ReloadItem> toReload;
		toReload.reserve(storage->size());

		for (const auto& id : storage->getAllIDs()) {
			auto it = assetParams.find(id);
			if (it != assetParams.end()) {
				toReload.push_back({id, it->second->clone()});
			}
		}

		size_t reloaded = 0;

		for (auto& item : toReload) {
			unload<AssetType>(item.id);

			if (load<AssetType>(item.id, *item.param))
				++reloaded;
		}

		return reloaded;
	}

	template <typename... Types>
	size_t reloadAllTyped() {
		size_t reloaded = 0;

		((reloaded += reloadAll<Types>()), ...);

		return reloaded;
	}

	template <typename AssetType>
	size_t getCount() const {
		auto* storage = getStorage<AssetType>();
		return storage ? storage->size() : 0;
	}

	template <typename AssetType>
	size_t getMemoryUsage() const {
		auto* storage = getStorage<AssetType>();
		return storage ? storage->getMemoryUsage() : 0;
	}

	size_t getTotalMemoryUsage() const {
		size_t total = 0;
		for (const auto& [type, storage] : storages)
			total += storage->getMemoryUsage();

		return total;
	}

	template <typename AssetType>
	std::vector<std::string> getAllIDs() const {
		auto* storage = getStorage<AssetType>();
		return storage ? storage->getAllIDs() : std::vector<std::string>{};
	}

private:
	template<typename AssetType>
	AssetStorage<AssetType>* getStorage() {
		std::type_index type = std::type_index(typeid(AssetType));
		auto it = storages.find(type);

		if (it == storages.end()) {
			storages[type] = std::make_unique<AssetStorage<AssetType>>();
			return static_cast<AssetStorage<AssetType>*>(storages[type].get());
		}

		return static_cast<AssetStorage<AssetType>*>(it->second.get());
	}

	template <typename AssetType>
	const AssetStorage<AssetType>* getStorage() const {
		return const_cast<AssetManager*>(this)->getStorage<AssetType>();
	}

	class ILoaderWrapper {
	public:
		virtual ~ILoaderWrapper() = default;
		virtual bool load(AssetManager& manager, const std::string& id, const LoadParams& params) = 0;
		virtual std::vector<std::string> getSupportedExtensions() const = 0;
	};

	template <typename AssetType>
	class LoaderWrapper : public ILoaderWrapper {
	public:
		LoaderWrapper(std::unique_ptr<IAssetLoader<AssetType>> l)
			: loader(std::move(l))
		{}

		bool load(AssetManager& manager, const std::string& id, const LoadParams& params) override {
			if (manager.has<AssetType>(id))
				return true;

			auto asset = loader->load(params);

			if (!asset)
				return false;

			manager.getStorage<AssetType>()->add(id, std::move(asset));
			return true;
		}

		std::vector<std::string> getSupportedExtensions() const override {
			return loader->getSupportedExtensions();
		}

	private:
		std::unique_ptr<IAssetLoader<AssetType>> loader;
	};

	std::unordered_map<std::type_index, std::unique_ptr<IAssetStorage>> storages;
	std::unordered_map<std::type_index, std::unique_ptr<ILoaderWrapper>> loaders;

	std::unordered_map<std::string, std::unique_ptr<LoadParams>> assetParams;

	std::unordered_map<std::string, std::string> aliases;

};

} // namespace Assets
