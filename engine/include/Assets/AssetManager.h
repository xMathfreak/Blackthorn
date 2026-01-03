#pragma once

#include "Assets/AssetHandle.h"
#include "Assets/AssetStorage.h"
#include "Assets/IAssetLoader.h"
#include "Assets/IAssetStorage.h"
#include "Core/Export.h"

#include <filesystem>
#include <string>
#include <typeindex>
#include <unordered_map>

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
	AssetType* load(const std::string& id, const std::string& path) {
		std::type_index type = std::type_index(typeid(AssetType));

		if (has<AssetType>(id))
			return get<AssetType>(id);

		auto loaderIt = loaders.find(type);
		if (loaderIt == loaders.end())
			return nullptr;

		LoaderWrapper<AssetType>* lw = static_cast<LoaderWrapper<AssetType>*>(loaderIt->second.get());
		std::unique_ptr<AssetType> asset = lw->loader->load(path);

		if (!asset)
			return nullptr;

		AssetType* rawPtr = asset.get();
		getStorage<AssetType>()->add(id, std::move(asset));

		assetPaths[id] = path;
		pathToID[path] = id;

		return rawPtr;
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
			assetPaths.erase(id);
		}
	}

	template <typename AssetType>
	void unloadAll() {
		auto* storage = getStorage<AssetType>();
		if (storage) {
			for (const auto& id : storage->getAllIDs())
				assetPaths.erase(id);

			storage->clear();
		}
	}

	void clear() {
		for (auto& [type, storage] : storages)
			storage->clear();

		assetPaths.clear();
		pathToID.clear();
		aliases.clear();
	}

	template <typename AssetType>
	bool reload(const std::string& id) {
		auto pathIt = assetPaths.find(id);
		if (pathIt == assetPaths.end())
			return false;

		std::string path = pathIt->second;
		unload<AssetType>(id);

		return load<AssetType>(id, path) != nullptr;
	}

	template <typename AssetType>
	size_t reloadAll() {
		auto* storage = getStorage<AssetType>();

		if (!storage)
			return 0;

		std::vector<std::pair<std::string, std::string>> toReload;

		for (const auto& id : storage->getAllIDs()) {
			auto pathIt = assetPaths.find(id);
			if (pathIt != assetPaths.end())
				toReload.emplace_back(id, pathIt->second);
		}

		size_t reloaded = 0;

		for (const auto& [id, path] : toReload) {
			unload<AssetType>(id);
			if (load<AssetType>(id, path) != nullptr)
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
		return storage ? storage->getAllIDs : std::vector<std::string>{};
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
	};

	template <typename AssetType>
	class LoaderWrapper : public ILoaderWrapper {
	public:
		LoaderWrapper(std::unique_ptr<IAssetLoader<AssetType>> l)
			: loader(std::move(l))
		{}

		std::unique_ptr<IAssetLoader<AssetType>> loader;
	};

	std::unordered_map<std::type_index, std::unique_ptr<IAssetStorage>> storages;
	std::unordered_map<std::type_index, std::unique_ptr<ILoaderWrapper>> loaders;

	std::unordered_map<std::string, std::string> assetPaths;
	std::unordered_map<std::string, std::string> pathToID;

	std::unordered_map<std::string, std::string> aliases;

};

} // namespace Assets
