#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <typeindex>
#include <unordered_map>

#include "Assets/AssetResolver.h"
#include "AssetNormalize.h"
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
 * @brief Default asset ID for a path-only load: the normalized path.
 *
 * Paths that fail normalization fall back to their generic string so the load
 * still reports a useful error.
 */
inline std::string defaultAssetID(const std::filesystem::path& p) {
	const std::string generic = p.generic_string();

	if (auto norm = Blackthorn::normalizeAssetPath(generic))
		return *norm;

	return generic;
}

/**
 * @class AssetManager
 * @brief Unified synchronous and asynchronous asset manager.
 *
 * Every entry point returns an AssetHandle, including get(). A handle reports
 * its asset's status, and get() on a handle returns a pointer only while the
 * asset is Ready.
 *
 * @section paths Paths and IDs
 * Asset paths are relative to the project root, which is also the runtime
 * working directory. The same string names the asset in loose-file and pack
 * mode:
 * @code
 * auto tex = manager.load<Texture>("assets/textures/player.png");
 * @endcode
 * Path-only loads use the normalized path as the ID.
 *
 * @section statuses Status and failures
 * A failed load is remembered as Failed until the same ID is loaded or reloaded
 * again. A load that succeeds clears the failure. Unload clears it too.
 *
 * @section registration Registration
 * Loaders get the resolver injected at registration, so constructors take no
 * resolver argument.
 * @code
 * manager.registerLoader<Texture>(
 *     std::make_unique<TextureLoader>(),
 *     std::make_unique<AsyncTextureLoader>()
 * );
 * @endcode
 *
 * @section flushing Flushing
 * Call flushPendingUploads() once per frame. From a loading screen or scene
 * transition, block until all loads finish with flushAllPendingUploads().
 *
 * @section threading Threading
 * All methods except the internal worker jobs must be called from the main
 * thread. Only the decode stage runs on workers.
 */
class BLACKTHORN_API AssetManager {
public:
	/**
	 * @brief Constructs the AssetManager.
	 *
	 * @param js The engine's JobSystem. Must outlive this AssetManager.
	 */
	explicit AssetManager(Jobs::JobSystem& js);
	~AssetManager();

	AssetManager(const AssetManager&) = delete;
	AssetManager& operator=(const AssetManager&) = delete;

	/**
	 * @brief Registers a synchronous loader and optionally an async loader.
	 *
	 * @tparam AssetType The asset type both loaders produce.
	 */
	template <typename AssetType>
	void registerLoader(
		std::unique_ptr<IAssetLoader<AssetType>> syncLoader,
		std::unique_ptr<IAsyncAssetLoader<AssetType>> asyncLoader = nullptr
	) {
		const auto type = std::type_index(typeid(AssetType));

		if (syncLoader)
			syncLoader->setResolver(&assetResolver);
		if (asyncLoader)
			asyncLoader->setResolver(&assetResolver);

		loaders[type] = std::make_unique<LoaderWrapper<AssetType>>(
			std::move(syncLoader),
			std::move(asyncLoader)
		);

		if (!storages.count(type))
			storages.try_emplace(type, std::make_unique<AssetStorage<AssetType>>());
	}

	/**
	 * @brief Synchronous, blocking load.
	 *
	 * @return A handle that is Ready on success or Failed on failure.
	 */
	template <typename AssetType>
	AssetHandle<AssetType> load(const std::string& id, const LoadParams& params) {
		if (has<AssetType>(id))
			return AssetHandle<AssetType>(id, this);

		const auto type = std::type_index(typeid(AssetType));
		auto it = loaders.find(type);
		if (it == loaders.end()) {
			BT_WARN("AssetManager: no loader registered for '{}' (id '{}')",
				typeid(AssetType).name(), id);
			recordFailure(id, type);
			return AssetHandle<AssetType>(id, this);
		}

		auto* wrapper = static_cast<LoaderWrapper<AssetType>*>(it->second.get());
		auto asset = wrapper->syncLoader ? wrapper->syncLoader->load(params) : nullptr;

		if (!asset) {
			BT_ERROR("AssetManager: sync load failed for '{}'", id);
			recordFailure(id, type);
			return AssetHandle<AssetType>(id, this);
		}

		getStorage<AssetType>()->add(id, std::move(asset));
		assetParams[id] = params.clone();
		forgetFailure(id);

		return AssetHandle<AssetType>(id, this);
	}

	/** @brief Synchronous load with an explicit ID. */
	template <typename AssetType>
	AssetHandle<AssetType> load(const std::string& id, std::filesystem::path path) {
		return load<AssetType>(id, AssetLoadParams(std::move(path)));
	}

	/** @brief Synchronous load; the ID is the normalized path. */
	template <typename AssetType>
	AssetHandle<AssetType> load(std::filesystem::path path) {
		std::string id = defaultAssetID(path);
		return load<AssetType>(id, AssetLoadParams(std::move(path)));
	}

	/**
	 * @brief Asynchronous, non-blocking load.
	 *
	 * Submits a decode job on a worker, chained to an upload job on the main
	 * thread. The upload runs during flushPendingUploads(). The returned handle
	 * is Pending until then.
	 *
	 * If no async loader is registered, falls back to load().
	 */
	template <typename AssetType>
	AssetHandle<AssetType> loadAsync(const std::string& id, const LoadParams& params) {
		if (has<AssetType>(id))
			return AssetHandle<AssetType>(id, this);

		const auto type = std::type_index(typeid(AssetType));
		auto it = loaders.find(type);
		if (it == loaders.end()) {
			BT_WARN("AssetManager: no loader registered for '{}' (id '{}')",
				typeid(AssetType).name(), id);
			recordFailure(id, type);
			return AssetHandle<AssetType>(id, this);
		}

		auto* wrapper = static_cast<LoaderWrapper<AssetType>*>(it->second.get());

		if (!wrapper->asyncLoader) {
			BT_DEBUG("AssetManager: no async loader for '{}', falling back to sync", id);
			return load<AssetType>(id, params);
		}

		{
			std::unique_lock<std::mutex> lock(inFlightMutex);

			// Already pending: the existing entry keeps its type, so a
			// different type asking for the same ID sees Missing, not Pending.
			if (inFlight.count(id))
				return AssetHandle<AssetType>(id, this);

			failed.erase(id);
			inFlight.emplace(id, InFlightEntry{ Jobs::JobHandle::create(), type });
		}

		++pendingTotal;
		assetParams[id] = params.clone();

		Jobs::JobHandlePtr decodeHandle = jobs.createHandle();
		Jobs::JobHandlePtr uploadHandle;

		{
			std::unique_lock<std::mutex> lock(inFlightMutex);
			uploadHandle = inFlight.at(id).job;
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
			[this, id, type, loaderWrapper, uploadHandle, decodeHandle]() {
				auto* raw = decodeHandle->getOutput<IRawAssetData>();

				bool stored = false;
				if (raw) {
					stored = loaderWrapper->upload(*raw, *this);
					delete raw;
				}

				{
					std::unique_lock<std::mutex> lk(inFlightMutex);
					if (!stored)
						failed.insert_or_assign(id, type);

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

		return AssetHandle<AssetType>(id, this);
	}

	/** @brief Asynchronous load with an explicit ID. */
	template <typename AssetType>
	AssetHandle<AssetType> loadAsync(const std::string& id, std::filesystem::path path) {
		return loadAsync<AssetType>(id, AssetLoadParams(std::move(path)));
	}

	/** @brief Asynchronous load; the ID is the normalized path. */
	template <typename AssetType>
	AssetHandle<AssetType> loadAsync(std::filesystem::path path) {
		std::string id = defaultAssetID(path);
		return loadAsync<AssetType>(id, AssetLoadParams(std::move(path)));
	}

	/**
	 * @brief Returns a handle for @p id. Never fails; check status() or operator bool.
	 *
	 * Does not load anything. An ID that was never requested returns a Missing handle.
	 */
	template <typename AssetType>
	AssetHandle<AssetType> get(const std::string& id) {
		return AssetHandle<AssetType>(id, this);
	}

	/** @copydoc get */
	template <typename AssetType>
	AssetHandle<AssetType> get(const std::string& id) const {
		// The handle never mutates the manager, and queries are const in practice.
		return AssetHandle<AssetType>(id, const_cast<AssetManager*>(this));
	}

	/**
	 * @brief Raw pointer to a stored asset, or nullptr if it isn't Ready.
	 *
	 * Used by AssetHandle::get(). Prefer get() and a handle for normal code.
	 */
	template <typename AssetType>
	AssetType* peek(const std::string& id) {
		auto* storage = getStorage<AssetType>();
		return (storage && storage->has(id)) ? storage->get(id) : nullptr;
	}

	/**
	 * @brief Status of @p id for asset type @p AssetType.
	 *
	 * Ready wins over Pending. A pending or failed entry of a different type
	 * reports Missing.
	 */
	template <typename AssetType>
	AssetStatus statusOf(const std::string& id) const {
		if (has<AssetType>(id))
			return AssetStatus::Ready;

		const auto type = std::type_index(typeid(AssetType));

		std::unique_lock<std::mutex> lock(inFlightMutex);

		if (auto it = inFlight.find(id); it != inFlight.end())
			return it->second.type == type ? AssetStatus::Pending : AssetStatus::Missing;

		if (auto it = failed.find(id); it != failed.end() && it->second == type)
			return AssetStatus::Failed;

		return AssetStatus::Missing;
	}

	template <typename AssetType>
	bool has(const std::string& id) const {
		auto* storage = getStorage<AssetType>();
		return storage && storage->has(id);
	}

	template <typename AssetType>
	void add(const std::string& id, std::unique_ptr<AssetType> asset) {
		getStorage<AssetType>()->add(id, std::move(asset));
		forgetFailure(id);
	}

	template <typename AssetType>
	void unload(const std::string& id) {
		auto* storage = getStorage<AssetType>();
		if (storage) {
			storage->remove(id);
			assetParams.erase(id);
		}
		forgetFailure(id);
	}

	template <typename AssetType>
	void unloadAll() {
		auto* storage = getStorage<AssetType>();
		if (storage) {
			for (const auto& id : storage->getAllIDs()) {
				assetParams.erase(id);
				forgetFailure(id);
			}
			storage->clear();
		}
	}

	void clear() {
		for (auto& [type, storage] : storages)
			storage->clear();
		assetParams.clear();

		std::unique_lock<std::mutex> lock(inFlightMutex);
		failed.clear();
	}

	/**
	 * @brief Reloads an asset from its stored parameters.
	 *
	 * Also the way to retry a Failed asset whose parameters were recorded. Returns
	 * false if no parameters are stored for @p id.
	 */
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

	void unloadByID(const std::string& id) {
		for (auto& [type, storage] : storages) {
			if (storage->has(id))
				storage->remove(id);
		}
		assetParams.erase(id);
		forgetFailure(id);
	}

	/**
	 * @brief Promotes completed decode jobs into GPU-resident assets.
	 *
	 * Call once per frame before beginScene().
	 */
	void flushPendingUploads();

	/**
	 * @brief Blocks until all outstanding async loads are complete.
	 *
	 * Only call from loading screens or scene transitions. This is the only
	 * way to wait for loads to finish, because uploads run on the main thread.
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
	 * The last pack mounted wins any ID conflict, giving mod packs priority
	 * over base game packs.
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
	 */
	void unmountPack(const std::filesystem::path& path) {
		assetResolver.unmount(path);
	}

	/**
	 * @brief Returns the metadata of the pack mounted at @p path.
	 *
	 * @param path Path that was previously passed to mountPack().
	 * @return The pack's PackMetadata, or std::nullopt if no pack is mounted there.
	 */
	std::optional<PackMetadata> getPackMetadata(const std::filesystem::path& path) const {
		return assetResolver.getPackMetadata(path);
	}

#endif

	/// The resolver used for every asset read, in both build modes.
	AssetResolver& resolver() { return assetResolver; }
	const AssetResolver& resolver() const { return assetResolver; }

private:
	struct InFlightEntry {
		Jobs::JobHandlePtr job;
		std::type_index    type;
	};

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

	void recordFailure(const std::string& id, std::type_index type) {
		std::unique_lock<std::mutex> lock(inFlightMutex);
		failed.insert_or_assign(id, type);
	}

	void forgetFailure(const std::string& id) {
		std::unique_lock<std::mutex> lock(inFlightMutex);
		failed.erase(id);
	}

	struct ILoaderWrapper {
		virtual ~ILoaderWrapper() = default;
		virtual std::unique_ptr<IRawAssetData> loadRaw(const LoadParams&) = 0;

		/// @return true if the asset is stored after the upload.
		virtual bool upload(IRawAssetData&, AssetManager&) = 0;
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

		bool upload(IRawAssetData& raw, AssetManager& manager) override {
			if (!asyncLoader)
				return false;

			asyncLoader->upload(raw, manager);
			return manager.has<AssetType>(raw.assetID);
		}

		std::unique_ptr<IAssetLoader<AssetType>> syncLoader;
		std::unique_ptr<IAsyncAssetLoader<AssetType>> asyncLoader;
	};

	std::unordered_map<std::type_index, std::unique_ptr<ILoaderWrapper>> loaders;
	std::unordered_map<std::type_index, std::unique_ptr<IAssetStorage>> storages;
	std::unordered_map<std::string, std::unique_ptr<LoadParams>> assetParams; ///< Asset parameters for reloading.

	AssetResolver assetResolver;

	Jobs::JobSystem& jobs;

	/// Pending async loads, keyed by ID. Guarded by inFlightMutex.
	std::unordered_map<std::string, InFlightEntry> inFlight;

	/// Failed loads, keyed by ID, with the type that failed. Guarded by inFlightMutex.
	std::unordered_map<std::string, std::type_index> failed;

	mutable std::mutex inFlightMutex;

	std::atomic<size_t> pendingTotal { 0 };
};

} // namespace Blackthorn::Assets

#include "Assets/AssetHandle.inl"
