#pragma once

#include <atomic>
#include <filesystem>
#include <format>
#include <memory>
#include <mutex>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>

#include "Core/Export.h"
#include "Assets/AssetHandle.h"
#include "Assets/AssetStorage.h"
#include "Assets/IAssetLoader.h"
#include "Assets/IAssetStorage.h"
#include "Assets/LoadParams.h"
#include "Assets/RawAssetData.h"
#include "Threads/ThreadPool.h"
#include "Debug/Logger.h"

namespace Blackthorn::Assets {

// ---------------------------------------------------------------------------
// AssetManager
// ---------------------------------------------------------------------------
// Unified synchronous + asynchronous asset manager.
//
// Both load paths share the same typed AssetStorage<T>, so an asset loaded
// by either path is accessible via the same get<T>(id) call. The only
// difference between the two paths from the caller's perspective is whether
// the returned handle is immediately ready.
//
// Registration:
//   // Sync only — async path not available for this type:
//   manager.registerLoader<Texture>(make_unique<TextureLoader>());
//
//   // Sync + async — both paths available:
//   manager.registerLoader<Texture>(
//       make_unique<TextureLoader>(),
//       make_unique<AsyncTextureLoader>()
//   );
//
// Loading:
//   AssetHandle<Texture> h1 = manager.load<Texture>("id", params);   // ready immediately
//   AssetHandle<Texture> h2 = manager.loadAsync<Texture>("id", p);   // ready later
//
//   if (h2.isReady()) { Texture* t = h2.get(); }
//   h2.wait();           // block until ready (loading screens only)
//
// Flushing (call once per frame before beginScene, or call flushAll from
// a loading screen):
//   manager.flushPendingUploads(4);
//   manager.flushAllPendingUploads();
// ---------------------------------------------------------------------------
class BLACKTHORN_API AssetManager {
public:
	// workerCount == 0  →  max(1, hardware_concurrency - 1) worker threads.
	explicit AssetManager(size_t workerCount = 0);
	~AssetManager() = default;

	AssetManager(const AssetManager&)            = delete;
	AssetManager& operator=(const AssetManager&) = delete;

	/**
	 * @brief Register the sync loader (required) and optionally an async loader.
	 *
	 * @param syncLoader The synchronous loader.
	 * @param asyncLoader The asynchronous loader.
	 *
	 * @tparam AssetType The type of asset for the corresponding loader.
	 * @note This should be called before any load() / loadAsync() for AssetType
	 */
	template <typename AssetType>
	void registerLoader(
		std::unique_ptr<IAssetLoader<AssetType>> syncLoader,
		std::unique_ptr<IAsyncAssetLoader<AssetType>> asyncLoader = nullptr
	) {
		auto type = std::type_index(typeid(AssetType));

		loaders[type] = std::make_unique<LoaderWrapper<AssetType>>(
			std::move(syncLoader),
			std::move(asyncLoader)
		);

		if (storages.find(type) == storages.end())
			storages[type] = std::make_unique<AssetStorage<AssetType>>();
	}

	/**
	 * @brief Synchronous, blocking load for an asset of type AssetType.
	 *
	 * @param id The asset identifier.
	 * @param params The parameters for loading the asset.
	 *
	 * @tparam AssetType The type of asset to be loaded.
	 *
	 * @return A ready AssetHandle to the loaded asset, or an invalid handle if loading fails or no loader is associated with AssetType.
	 */
	template <typename AssetType>
	AssetHandle<AssetType> load(const std::string& id, const LoadParams& params) {
		if (has<AssetType>(id))
			return makeReadyHandle<AssetType>(id);

		auto type = std::type_index(typeid(AssetType));
		auto it = loaders.find(type);
		if (it == loaders.end()) {
			BT_WARN(std::format("AssetManager: no loader registered for '{}' (id '{}')",
				typeid(AssetType).name(), id));
			return {};
		}

		auto* wrapper = static_cast<LoaderWrapper<AssetType>*>(it->second.get());
		auto asset = wrapper->syncLoader->load(params);
		if (!asset) {
			BT_ERROR(std::format("AssetManager: sync load failed for '{}'", id));
			return {};
		}

		getStorage<AssetType>()->add(id, std::move(asset));
		assetParams[id] = params.clone();

		return makeReadyHandle<AssetType>(id);
	}

	template <typename AssetType>
	AssetHandle<AssetType> load(const std::string& id, const std::string& path) {
		return load<AssetType>(id, PathLoadParams(path));
	}

	template <typename AssetType>
	AssetHandle<AssetType> load(const std::string& path) {
		return load<AssetType>(std::filesystem::path(path).stem().string(), PathLoadParams(path));
	}

	/**
	 * @brief Asynchronous, non-blocking load for an asset of type AssetType.
	 *
	 * @param id The asset identifier.
	 * @param params The parameters for loading the asset.
	 *
	 * @tparam AssetType The type of asset to be loaded.
	 *
	 * @return A pending AssetHandle for the asset that becomes ready when the asset is loaded.
	 * @note If no async loader is registered for AsetType, falls back to a synchronous load.
	 */
	template <typename AssetType>
	AssetHandle<AssetType> loadAsync(const std::string& id, const LoadParams& params) {
		if (has<AssetType>(id))
			return makeReadyHandle<AssetType>(id);

		auto type  = std::type_index(typeid(AssetType));
		auto it    = loaders.find(type);
		if (it == loaders.end()) {
			BT_WARN(std::format("AssetManager: no loader registered for '{}' (id '{}')",
				typeid(AssetType).name(), id));
			return {};
		}

		auto* wrapper = static_cast<LoaderWrapper<AssetType>*>(it->second.get());

		if (!wrapper->asyncLoader) {
			BT_DEBUG(std::format(
				"AssetManager: no async loader for '{}', falling back to sync", id));
			return load<AssetType>(id, params);
		}

		{
			std::unique_lock<std::mutex> lock(inFlightMutex);
			if (inFlight.count(id)) {
				BT_DEBUG(std::format("AssetManager: '{}' already in-flight", id));
				return makePendingHandle<AssetType>(id);
			}

			inFlight.insert(id);
		}

		++pendingTotal;

		auto readyFlag  = std::make_shared<std::atomic<bool>>(false);
		auto paramsCopy = params.clone();

		ILoaderWrapper* loaderWrapper = it->second.get();

		assetParams[id] = params.clone();

		threadPool.enqueue([this, id, loaderWrapper, paramsCopy = std::move(paramsCopy), readyFlag]() {
			auto raw = loaderWrapper->loadRaw(*paramsCopy);

			if (!raw || !raw->valid) {
				BT_ERROR(std::format("AssetManager: loadRaw failed for '{}'", id));

				{
					std::unique_lock<std::mutex> lk(inFlightMutex);
					inFlight.erase(id);
				}

				--pendingTotal;
				return;
			}

			raw->assetID = id;

			pushUpload([this, loaderWrapper, raw = std::move(raw), readyFlag]() mutable {
				loaderWrapper->upload(*raw, *this);

				{
					std::unique_lock<std::mutex> lk(inFlightMutex);
					inFlight.erase(raw->assetID);
				}

				readyFlag->store(true, std::memory_order_release);
				--pendingTotal;
			});
		});

		return AssetHandle<AssetType>(id, this, std::move(readyFlag));
	}

	template <typename AssetType>
	AssetHandle<AssetType> loadAsync(const std::string& id, const std::string& path) {
		return loadAsync<AssetType>(id, PathLoadParams(path));
	}

	template <typename AssetType>
	AssetType* get(const std::string& id) {
		auto* storage = getStorage<AssetType>();
		if (!storage || !storage->has(id))
			return nullptr;
		return storage->get(id).get();
	}

	template <typename AssetType>
	const AssetType* get(const std::string& id) const {
		return const_cast<AssetManager*>(this)->get<AssetType>(id);
	}

	template <typename AssetType>
	bool has(const std::string& id) const {
		auto* storage = getStorage<AssetType>();
		return storage && storage->has(id);
	}

	template <typename AssetType>
	void add(const std::string& id, std::unique_ptr<AssetType> asset) {
		getStorage<AssetType>()->add(id, std::move(asset));
	}

	template <typename AssetType>
	void unload(const std::string& id) {
		auto* storage = getStorage<AssetType>();
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
	}

	template <typename AssetType>
	bool reload(const std::string& id) {
		auto it = assetParams.find(id);
		if (it == assetParams.end())
			return false;
		auto paramsCopy = it->second->clone();
		unload<AssetType>(id);
		return static_cast<bool>(load<AssetType>(id, *paramsCopy));
	}

	template <typename AssetType>
	size_t getCount() const {
		auto* s = getStorage<AssetType>();
		return s ? s->size() : 0;
	}

	size_t getTotalMemoryUsage() const {
		size_t total = 0;
		for (const auto& [type, storage] : storages)
			total += storage->getMemoryUsage();
		return total;
	}

	/**
	 * @brief Processes at most `uploadBudget` uploads.
	 *
	 * Call at the start of each frame, before beginScene().
	 *
	 * @param[in] uploadBudget The maximum number of assets to upload.
	 *
	 * @return Number of processed assets.
	 */
	size_t flushPendingUploads(size_t uploadBudget = 4);

	// Block until all worker decode jobs finish, then drain the entire upload
	// queue. Only call this from loading screens / scene transitions.
	void flushAllPendingUploads();

	/// Total outstanding loads (in thread pool + in upload queue).
	size_t pendingCount() const;

private:
	template <typename AssetType>
	AssetStorage<AssetType>* getStorage() {
		auto type = std::type_index(typeid(AssetType));
		auto it = storages.find(type);
		if (it == storages.end()) {
			storages[type] = std::make_unique<AssetStorage<AssetType>>();
			return static_cast<AssetStorage<AssetType>*>(storages[type].get());
		}
		return static_cast<AssetStorage<AssetType>*>(it->second.get());
	}

	template <typename AssetType>
	const AssetStorage<AssetType>* getStorage() const {
		auto type = std::type_index(typeid(AssetType));
		auto it = storages.find(type);
		return (it != storages.end())
			? static_cast<const AssetStorage<AssetType>*>(it->second.get())
			: nullptr;
	}

	template <typename AssetType>
	AssetHandle<AssetType> makeReadyHandle(const std::string& id) {
		auto flag = std::make_shared<std::atomic<bool>>(true);
		return AssetHandle<AssetType>(id, this, std::move(flag));
	}

	/// Returns a handle whose flag is permanently false.
	template <typename AssetType>
	AssetHandle<AssetType> makePendingHandle(const std::string& id) {
		auto flag = std::make_shared<std::atomic<bool>>(false);
		return AssetHandle<AssetType>(id, this, std::move(flag));
	}

	struct ILoaderWrapper {
		virtual ~ILoaderWrapper() = default;
		virtual std::unique_ptr<IRawAssetData> loadRaw(const LoadParams&) = 0;
		virtual void upload(IRawAssetData&, AssetManager&) = 0;
	};

	template <typename AssetType>
	struct LoaderWrapper : ILoaderWrapper {
		LoaderWrapper(
			std::unique_ptr<IAssetLoader<AssetType>> sync,
			std::unique_ptr<IAsyncAssetLoader<AssetType>> async)
			: syncLoader(std::move(sync))
			, asyncLoader(std::move(async)
		) {}

		std::unique_ptr<IRawAssetData> loadRaw(const LoadParams& params) override {
			return asyncLoader ? asyncLoader->loadRaw(params) : nullptr;
		}

		void upload(IRawAssetData& raw, AssetManager& manager) override {
			if (asyncLoader)
				asyncLoader->upload(raw, manager);
		}

		std::unique_ptr<IAssetLoader<AssetType>> syncLoader;
		std::unique_ptr<IAsyncAssetLoader<AssetType>> asyncLoader;
	};

	template <typename Callable>
	requires std::invocable<Callable>
	void pushUpload(Callable&& callable) {
		std::unique_lock<std::mutex> lock(uploadMutex);
		uploadQueue.push(Threads::makeTask(std::forward<Callable>(callable)));
	}

	bool processOneUpload();

	std::unordered_map<std::type_index, std::unique_ptr<ILoaderWrapper>> loaders;
	std::unordered_map<std::type_index, std::unique_ptr<IAssetStorage>> storages;
	std::unordered_map<std::string, std::unique_ptr<LoadParams>> assetParams;

	Threads::ThreadPool threadPool;

	std::unordered_set<std::string>	inFlight;
	mutable std::mutex inFlightMutex;

	std::queue<Threads::TaskPtr> uploadQueue;
	mutable std::mutex uploadMutex;

	std::atomic<size_t> pendingTotal{ 0 };
};

} // namespace Blackthorn::Assets

// Inline AssetHandle::get<AssetType> definition
#include "Assets/AssetHandle.inl"