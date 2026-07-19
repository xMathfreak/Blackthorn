#pragma once

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <typeindex>
#include <unordered_map>

#ifdef BT_PACK_MODE
	#include "Assets/AssetResolver.h"
#endif

#include "Core/Export.h"
#include "Assets/AssetHandle.h"
#include "Assets/AssetStorage.h"
#include "Assets/IAssetLoader.h"
#include "Assets/IAssetStorage.h"
#include "Assets/LoadParams.h"
#include "Assets/RawAssetData.h"
#include "Debug/Logger.h"
#include "Jobs/JobSystem.h"

namespace Blackthorn::Assets {

/**
 * @class AssetManager
 * @brief Unified synchronous and asynchronous asset manager.
 *
 * Both load paths share the same typed AssetStorage<T>, so an asset loaded
 * by either path is accessible via the same get<T>(id) call. The only
 * difference between the two paths from the caller's perspective is whether
 * the returned handle is immediately ready.
 *
 * @section registration Registration
 * @code
 * // Sync only:
 * manager.registerLoader<Texture>(make_unique<TextureLoader>());
 *
 * // Sync + async (debug/loose-file mode):
 * manager.registerLoader<Texture>(
 *     make_unique<TextureLoader>(),
 *     make_unique<AsyncTextureLoader>()
 * );
 *
 * // Sync + async (BT_PACK_MODE resolver injected automatically):
 * manager.registerPackLoader<Texture>(
 *     make_unique<TextureLoader>(),
 *     make_unique<AsyncTextureLoader>(&manager.resolver())
 * );
 * @endcode
 *
 * @section loading Loading
 * @code
 * // Debug mode: identify by path
 * AssetHandle<Texture> h1 = manager.load<Texture>("player", PathLoadParams("assets/player.png"));
 *
 * // Pack mode: identify by pack ID
 * AssetHandle<Texture> h2 = manager.loadAsync<Texture>("player", PackLoadParams("player_png"));
 * @endcode
 *
 * @section flushing Flushing
 * Call once per frame to promote completed decode jobs into GPU-resident assets:
 * @code
 * manager.flushPendingUploads();
 * @endcode
 * From a loading screen or scene transition, block until all loads are done:
 * @code
 * manager.flushAllPendingUploads();
 * @endcode
 */
class BLACKTHORN_API AssetManager {
public:
	/**
	 * @brief Constructs the AssetManager.
	 *
	 * @param js The engine's JobSystem. Must outlive this AssetManager.
	 */
	explicit AssetManager(Jobs::JobSystem& js);
	~AssetManager() = default;

	AssetManager(const AssetManager&) = delete;
	AssetManager& operator=(const AssetManager&) = delete;

	/**
	 * @brief Registers a synchronous loader and optionally an async loader.
	 *
	 * Use in debug/loose-file builds. The async loader, if provided, is
	 * responsible for sourcing the resolver itself in BT_PACK_MODE (use
	 * registerPackLoader instead to have the manager inject it automatically).
	 *
	 * @tparam AssetType  The asset type both loaders produce.
	 */
	template <typename AssetType>
	void registerLoader(
		std::unique_ptr<IAssetLoader<AssetType>> syncLoader,
		std::unique_ptr<IAsyncAssetLoader<AssetType>> asyncLoader = nullptr
	) {
		const auto type = std::type_index(typeid(AssetType));

		loaders[type] = std::make_unique<LoaderWrapper<AssetType>>(
			std::move(syncLoader),
			std::move(asyncLoader)
		);

		if (!storages.count(type))
			storages.try_emplace(type, std::make_unique<AssetStorage<AssetType>>());
	}

#ifdef BT_PACK_MODE
	/**
	 * @brief Registers loaders for BT_PACK_MODE, injecting the AssetResolver
	 *        into the async loader automatically.
	 *
	 * Async loaders in pack mode require a non-owning pointer to the
	 * AssetResolver so they can call resolver.resolve(id) from the worker
	 * thread. This overload passes &assetResolver at registration time so
	 * callers never have to touch the resolver directly.
	 *
	 * Usage:
	 * @code
	 * // The async loader's constructor must accept AssetResolver* as its
	 * // first argument. Any additional arguments follow.
	 * manager.registerPackLoader<Texture>(
	 *     std::make_unique<TextureLoader>(),
	 *     std::make_unique<AsyncTextureLoader>(&manager.resolver())
	 * );
	 * @endcode
	 *
	 * @tparam AssetType   The asset type both loaders produce.
	 */
	template <typename AssetType>
	void registerPackLoader(
		std::unique_ptr<IAssetLoader<AssetType>> syncLoader,
		std::unique_ptr<IAsyncAssetLoader<AssetType>> asyncLoader
	) {
		registerLoader<AssetType>(std::move(syncLoader), std::move(asyncLoader));
	}
#endif

	/**
	 * @brief Synchronous, blocking load for an asset of type AssetType.
	 *
	 * Returns immediately with a ready handle. Prefer loadAsync() for assets
	 * loaded during gameplay to avoid stalling the frame.
	 *
	 * In BT_PACK_MODE, pass PackLoadParams to identify the asset by its
	 * pack ID. PathLoadParams is only meaningful in debug (loose-file) builds.
	 *
	 * @param id     Runtime identifier stored in AssetStorage.
	 * @param params Load parameters (PathLoadParams or PackLoadParams).
	 */
	template <typename AssetType>
	AssetHandle<AssetType> load(const std::string& id, const LoadParams& params) {
		if (has<AssetType>(id))
			return makeReadyHandle<AssetType>(id);

		const auto type = std::type_index(typeid(AssetType));
		auto it = loaders.find(type);
		if (it == loaders.end()) {
			BT_WARN("AssetManager: no loader registered for '{}' (id '{}')",
				typeid(AssetType).name(), id);
			return {};
		}

		auto* wrapper = static_cast<LoaderWrapper<AssetType>*>(it->second.get());
		auto asset = wrapper->syncLoader->load(params);
		if (!asset) {
			BT_ERROR("AssetManager: sync load failed for '{}'", id);
			return {};
		}

		getStorage<AssetType>()->add(id, std::move(asset));
		assetParams[id] = params.clone();

		return makeReadyHandle<AssetType>(id);
	}

	template <typename AssetType>
	AssetHandle<AssetType> load(const std::string& id, std::filesystem::path path) {
		return load<AssetType>(id, PathLoadParams(std::move(path)));
	}

	template <typename AssetType>
	AssetHandle<AssetType> load(std::filesystem::path path) {
		return load<AssetType>(path.stem().string(), PathLoadParams(std::move(path)));
	}

	/**
	 * @brief Asynchronous, non-blocking load for an asset of type AssetType.
	 *
	 * Submits a worker-thread decode job followed by a main-thread upload job
	 * chained to it. The upload runs the next time flushPendingUploads() is
	 * called (once per frame).
	 *
	 * In BT_PACK_MODE, @p params must be PackLoadParams. PathLoadParams will
	 * cause the async loader to fail silently because the ID derivation from
	 * a file path does not match the ID written by gen-manifest/btpacker.
	 *
	 * @param id     Runtime identifier stored in AssetStorage.
	 * @param params Load parameters (PackLoadParams in pack mode).
	 */
	template <typename AssetType>
	AssetHandle<AssetType> loadAsync(const std::string& id, const LoadParams& params) {
		if (has<AssetType>(id))
			return makeReadyHandle<AssetType>(id);

		const auto type = std::type_index(typeid(AssetType));
		auto it = loaders.find(type);
		if (it == loaders.end()) {
			BT_WARN("AssetManager: no loader registered for '{}' (id '{}')",
				typeid(AssetType).name(), id);
			return {};
		}

		auto* wrapper = static_cast<LoaderWrapper<AssetType>*>(it->second.get());

		if (!wrapper->asyncLoader) {
			BT_DEBUG("AssetManager: no async loader for '{}', falling back to sync", id);
			return load<AssetType>(id, params);
		}

		{
			std::unique_lock<std::mutex> lock(inFlightMutex);
			auto flightIt = inFlight.find(id);
			if (flightIt != inFlight.end()) {
				BT_DEBUG("AssetManager: '{}' already in-flight", id);
				return AssetHandle<AssetType>(id, this, flightIt->second);
			}

			auto uploadHandle = Jobs::JobHandle::create();
			inFlight.emplace(id, uploadHandle);
		}

		++pendingTotal;

		assetParams[id] = params.clone();

		Jobs::JobHandlePtr decodeHandle = jobs.createHandle();
		Jobs::JobHandlePtr uploadHandle;

		{
			std::unique_lock<std::mutex> lock(inFlightMutex);
			uploadHandle = inFlight.at(id);
		}

		ILoaderWrapper* loaderWrapper = it->second.get();
		auto paramsCopy = std::shared_ptr<LoadParams>(params.clone().release());

		jobs.submit(Jobs::Job(
			[this, id, loaderWrapper, paramsCopy, decodeHandle]() {
				auto raw = loaderWrapper->loadRaw(*paramsCopy);

				if (!raw || !raw->valid) {
					BT_ERROR("AssetManager: loadRaw failed for '{}'", id);
					return;
				}

				raw->assetID = id;
				decodeHandle->setOutput(raw.release());
			},
			decodeHandle
		));

		jobs.submit(Jobs::Job(
			[this, id, loaderWrapper, uploadHandle, decodeHandle]() {
				auto* raw = decodeHandle->getOutput<IRawAssetData>();

				if (raw) {
					loaderWrapper->upload(*raw, *this);
					delete raw;
				}

				{
					std::unique_lock<std::mutex> lk(inFlightMutex);
					inFlight.erase(id);
				}

				--pendingTotal;

				uploadHandle->signal([this](std::function<void()> fn, bool isMt) {
					jobs.submit(Jobs::Job(
						std::move(fn),
						nullptr, nullptr,
						isMt ? Jobs::ThreadAffinity::MainThread : Jobs::ThreadAffinity::Any
					));
				});
			},
			nullptr,
			decodeHandle,
			Jobs::ThreadAffinity::MainThread
		));

		return AssetHandle<AssetType>(id, this, std::move(uploadHandle));
	}

	template <typename AssetType>
	AssetHandle<AssetType> loadAsync(const std::string& id, std::filesystem::path path) {
		return loadAsync<AssetType>(id, PathLoadParams(std::move(path)));
	}

	template <typename AssetType>
	AssetHandle<AssetType> loadAsync(std::filesystem::path path) {
		return loadAsync<AssetType>(path.stem().string(), PathLoadParams(std::move(path)));
	}

	template <typename AssetType>
	AssetType* get(const std::string& id) {
		auto* storage = getStorage<AssetType>();
		return (storage && storage->has(id)) ? storage->get(id) : nullptr;
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

	template <typename AssetType>
	std::vector<std::string> getSupportedExtensions() const {
		const auto type = std::type_index(typeid(AssetType));
		auto it = loaders.find(type);
		return (it != loaders.end())
			? it->second->getSupportedExtensions()
			: std::vector<std::string>{};
	}

	void unloadById(const std::string& id) {
		for (auto& [type, storage] : storages) {
			if (storage->has(id))
				storage->remove(id);
		}
		assetParams.erase(id);
	}

	/**
	 * @brief Promotes all completed decode jobs into GPU-resident assets.
	 *
	 * Call once per frame before beginScene().
	 */
	void flushPendingUploads();

	/**
	 * @brief Blocks until all outstanding async loads are complete.
	 *
	 * Only call from loading screens or scene transitions.
	 */
	void flushAllPendingUploads();

	/// Total outstanding loads (decode + upload jobs not yet complete).
	size_t pendingCount() const;

	/// Blocks until all outstanding async loads complete.
	void shutdown();

#ifdef BT_PACK_MODE
	/**
	 * @brief Mounts a .btp pack file onto the resolver stack.
	 *
	 * Call during engine startup before loading any packed assets. Safe to
	 * call at any time for runtime mod / DLC support.
	 *
	 * Mount order matters: the last pack mounted wins any ID conflict, giving
	 * mod packs priority over base game packs.
	 *
	 * @param path Path to the .btp file.
	 * @return true if the pack mounted successfully.
	 */
	bool mountPack(const std::filesystem::path& path) {
		return assetResolver.mount(path);
	}

	/**
	 * @brief Unmounts a previously mounted pack file.
	 *
	 * Assets already loaded from this pack remain alive in AssetStorage.
	 * Typical use: unmounting a chapter pack to reclaim TOC memory.
	 */
	void unmountPack(const std::filesystem::path& path) {
		assetResolver.unmount(path);
	}

	AssetResolver& resolver() { return assetResolver; }
	const AssetResolver& resolver() const { return assetResolver; }
#endif

private:
	template <typename AssetType>
	AssetStorage<AssetType>* getStorage() {
		const auto type = std::type_index(typeid(AssetType));
		auto it = storages.find(type);
		if (it == storages.end()) {
			storages[type] = std::make_unique<AssetStorage<AssetType>>();
			return static_cast<AssetStorage<AssetType>*>(storages[type].get());
		}
		return static_cast<AssetStorage<AssetType>*>(it->second.get());
	}

	template <typename AssetType>
	const AssetStorage<AssetType>* getStorage() const {
		const auto type = std::type_index(typeid(AssetType));
		auto it = storages.find(type);
		return (it != storages.end())
			? static_cast<const AssetStorage<AssetType>*>(it->second.get())
			: nullptr;
	}

	template <typename AssetType>
	AssetHandle<AssetType> makeReadyHandle(const std::string& id) {
		return AssetHandle<AssetType>(id, this, Jobs::JobHandle::createComplete());
	}

	struct ILoaderWrapper {
		virtual ~ILoaderWrapper() = default;
		virtual std::unique_ptr<IRawAssetData> loadRaw(const LoadParams&) = 0;
		virtual void upload(IRawAssetData&, AssetManager&) = 0;
		virtual std::vector<std::string> getSupportedExtensions() const = 0;
	};

	template <typename AssetType>
	struct LoaderWrapper : ILoaderWrapper {
		LoaderWrapper(
			std::unique_ptr<IAssetLoader<AssetType>> sync,
			std::unique_ptr<IAsyncAssetLoader<AssetType>> async)
			: syncLoader(std::move(sync))
			, asyncLoader(std::move(async))
		{}

		std::unique_ptr<IRawAssetData> loadRaw(const LoadParams& params) override {
			return asyncLoader ? asyncLoader->loadRaw(params) : nullptr;
		}

		void upload(IRawAssetData& raw, AssetManager& manager) override {
			if (asyncLoader)
				asyncLoader->upload(raw, manager);
		}

		std::vector<std::string> getSupportedExtensions() const override {
			return syncLoader->getSupportedExtensions();
		}

		std::unique_ptr<IAssetLoader<AssetType>> syncLoader;
		std::unique_ptr<IAsyncAssetLoader<AssetType>> asyncLoader;
	};

	std::unordered_map<std::type_index, std::unique_ptr<ILoaderWrapper>> loaders;
	std::unordered_map<std::type_index, std::unique_ptr<IAssetStorage>> storages;
	std::unordered_map<std::string, std::unique_ptr<LoadParams>> assetParams;

#ifdef BT_PACK_MODE
	AssetResolver assetResolver;
#endif

	Jobs::JobSystem& jobs;

	std::unordered_map<std::string, Jobs::JobHandlePtr> inFlight;
	mutable std::mutex inFlightMutex;

	std::atomic<size_t> pendingTotal { 0 };
};

} // namespace Blackthorn::Assets

#include "Assets/AssetHandle.inl"
