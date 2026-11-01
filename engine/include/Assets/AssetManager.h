#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>

#include "AssetNormalize.h"
#include "Assets/AssetHandle.h"
#include "Assets/AssetRef.h"
#include "Assets/AssetResolver.h"
#include "Assets/AssetStatus.h"
#include "Assets/IAssetLoader.h"
#include "Assets/IStore.h"
#include "Assets/LoadParams.h"
#include "Assets/NameTable.h"
#include "Assets/RawAssetData.h"
#include "Assets/Store.h"
#include "Assets/TypeKey.h"
#include "Core/Export.h"
#include "Debug/Logger.h"
#include "Jobs/JobSystem.h"

namespace Blackthorn::Assets {

/**
 * @brief Default asset ID for a path-only load: the normalized path.
 *
 * Paths that fail normalization fall back to their generic string so the
 * load still reports a useful error.
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
 * Asset identity is an AssetRef<AssetType>: a POD interned name. Refs are cheap to
 * copy, hold no pointers, and resolve to nullptr when the asset is unloaded.
 * AssetHandle<AssetType> is a C++ convenience wrapper that pairs a ref with this manager.
 *
 * @section ids Identity
 * get() interns the ID and returns its ref. It never fails and never loads.
 * Unknown IDs resolve to Missing until loaded. Names are never freed.
 *
 * @section statuses Status and failures
 * A failed load is remembered as Failed until the same ref is loaded or
 * reloaded again. A successful load clears the failure. Unload clears both
 * the asset and the recorded params, so reload() only works on assets that
 * are currently loaded or failed.
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
	 * @param js The engine's JobSystem. Must outlive this AssetManager.
	 */
	explicit AssetManager(Jobs::JobSystem& js);

	/// Blocks until outstanding async loads finish, then releases all stores.
	~AssetManager();

	AssetManager(const AssetManager&) = delete;
	AssetManager& operator=(const AssetManager&) = delete;

	// ---- Identity -----------------------------------------------------------

	/// Interns @p id and returns its ref. Does not load.
	template <typename AssetType>
	AssetRef<AssetType> get(std::string_view id) {
		return AssetRef<AssetType>{ names.intern(id) };
	}

	/// Missing for null refs and for assets that are not loaded.
	template <typename AssetType>
	AssetStatus status(AssetRef<AssetType> ref) const {
		const auto* store = findStore<AssetType>();
		return store ? store->statusOf(ref.name) : AssetStatus::Missing;
	}

	/// The interned name for @p name, or empty for a null ref.
	/// The view stays valid for the manager's lifetime.
	std::string_view nameOf(U32 name) const {
		return name == U32_MAX ? std::string_view{} : names.nameOf(name);
	}

	// ---- Access -------------------------------------------------------------

	/// Non-null only when Ready. Valid until the asset is unloaded or reloaded.
	template <typename AssetType>
	AssetType* resolve(AssetRef<AssetType> ref) {
		auto* store = findStore<AssetType>();
		return store ? store->get(ref.name) : nullptr;
	}

	template <typename AssetType>
	const AssetType* resolve(AssetRef<AssetType> ref) const {
		const auto* store = findStore<AssetType>();
		return store ? store->get(ref.name) : nullptr;
	}

	template <typename AssetType>
	bool has(AssetRef<AssetType> ref) const {
		return resolve(ref) != nullptr;
	}

	template <typename AssetType>
	AssetHandle<AssetType> getHandle(AssetRef<AssetType> ref) {
		return AssetHandle<AssetType>(ref, this);
	}

	template <typename AssetType>
	AssetHandle<AssetType> getHandle(std::string_view name) {
		return AssetHandle<AssetType>(get<AssetType>(name), this);
	}

	// ---- Direct insertion and removal ----------------------------------------

	/// Stores an asset directly, marking it Ready. Cancels any pending load for the same ref.
	template <typename AssetType>
	void add(AssetRef<AssetType> ref, std::unique_ptr<AssetType> asset) {
		requireNamed(ref, "add");

		if (!asset)
			throw std::invalid_argument("AssetManager: Cannot add null asset");

		getStore<AssetType>().add(ref.name, std::move(asset));
	}

	void unload(std::string_view id) {
		const auto name = names.find(id);

		if (!name)
			return;

		for (auto& [key, store] : stores)
			store->unload(*name);
	}

	/// Drops the asset and its params, and cancels any pending load for the ref.
	template <typename AssetType>
	void unload(AssetRef<AssetType> ref) {
		if (auto* store = findStore<AssetType>())
			store->unload(ref.name);
	}

	template <typename AssetType>
	void unloadAll() {
		if (auto* store = findStore<AssetType>())
			store->clear();
	}

	void clear() {
		for (auto& [key, store] : stores)
			store->clear();
	}

	size_t getTotalMemoryUsage() const {
		size_t total = 0;

		for (const auto& [key, store] : stores)
			total += store->getMemoryUsage();

		return total;
	}

	// ---- Loading ------------------------------------------------------------

	/**
	 * @brief Synchronously loads an asset by path. The path is also the ID.
	 */
	template <typename AssetType>
	AssetRef<AssetType> load(std::filesystem::path path) {
		std::string id = defaultAssetID(path);
		return load(get<AssetType>(id), AssetLoadParams(std::move(path)));
	}

	template <typename AssetType>
	AssetRef<AssetType> load(std::string_view id, const LoadParams& params) {
		return load(get<AssetType>(id), params);
	}

	template <typename AssetType>
	AssetRef<AssetType> load(std::string_view id, std::string_view path) {
		return load(get<AssetType>(id), AssetLoadParams(std::move(path)));
	}

	/**
	 * @brief Synchronously loads @p ref. A no-op if the asset is already Ready.
	 *
	 * A pending async load for the same ref is cancelled, because the sync
	 * result bumps the slot generation.
	 */
	template <typename AssetType>
	AssetRef<AssetType> load(AssetRef<AssetType> ref, const LoadParams& params) {
		requireNamed(ref, "load");

		auto& store = getStore<AssetType>();

		if (store.get(ref.name))
			return ref;

		// Record params before the attempt, so a failed load can be reloaded.
		{
			auto& slot = store.ensure(ref.name);
			slot.params = params.clone();
			slot.async = false;
		}

		if (!store.hasSyncLoader()) {
			BT_DEBUG("AssetManager: missing sync loader for {}", typeid(AssetType).name());
			store.ensure(ref.name).status = AssetStatus::Failed;
			return ref;
		}

		auto asset = store.getSyncLoader()->load(params);

		if (!asset) {
			store.ensure(ref.name).status = AssetStatus::Failed;
			return ref;
		}

		store.add(ref.name, std::move(asset));
		return ref;
	}

	template <typename AssetType>
	AssetRef<AssetType> loadAsync(std::filesystem::path path) {
		std::string id = defaultAssetID(path);
		return loadAsync(get<AssetType>(id), AssetLoadParams(std::move(path)));
	}

	template <typename AssetType>
	AssetRef<AssetType> loadAsync(std::string_view id, std::string_view path) {
		return loadAsync(get<AssetType>(id), AssetLoadParams(std::move(path)));
	}

	template <typename AssetType>
	AssetRef<AssetType> loadAsync(std::string_view id, const LoadParams& params) {
		return loadAsync(get<AssetType>(id), params);
	}

	/**
	 * @brief Starts an async load of @p ref.
	 *
	 * A no-op if the asset is Ready or already Pending. Use reload() to refresh
	 * a loaded asset. Without an async loader, this falls back to load().
	 */
	template <typename AssetType>
	AssetRef<AssetType> loadAsync(AssetRef<AssetType> ref, const LoadParams& params) {
		requireNamed(ref, "loadAsync");

		auto& store = getStore<AssetType>();

		const AssetStatus current = store.statusOf(ref.name);
		if (current == AssetStatus::Ready || current == AssetStatus::Pending)
			return ref;

		if (!store.hasAsyncLoader()) {
			BT_DEBUG("AssetManager: missing async loader for {}, using synchronous loading", typeid(AssetType).name());
			return load(ref, params);
		}

		{
			auto& slot = store.ensure(ref.name);
			slot.params = params.clone();
			slot.async = true;
		}

		const U64 generation = store.beginPending(ref.name);
		++pendingTotal;

		auto* loader = store.getAsyncLoader();
		auto paramsCopy = std::shared_ptr<LoadParams>(params.clone());
		auto decodeHandle = jobs.createHandle();
		const U32 name = ref.name;

		// Decode runs on a worker and stores its raw output on the handle.
		jobs.submit(Jobs::Job(
			[loader, paramsCopy, decodeHandle, name]() {
				auto raw = loader->loadRaw(*paramsCopy);

				if (!raw || !raw->valid)
					return;

				raw->assetName = name;
				decodeHandle->setOutput(raw.release());
			},
			decodeHandle
		));

		// Upload runs on the main thread after decode completes. A load that was
		// unloaded or superseded in the meantime fails the generation check, and
		// its raw data is discarded.
		jobs.submit(Jobs::Job(
			[this, loader, decodeHandle, name, generation]() {
				std::unique_ptr<IRawAssetData> raw(
					decodeHandle->getOutput<IRawAssetData>()
				);

				if (auto* target = findStore<AssetType>(); target && target->isPending(name, generation)) {
					if (raw)
						loader->upload(*raw, *this);

					if (auto* after = findStore<AssetType>())
						after->markFailed(name, generation);
				}

				--pendingTotal;
			},
			nullptr,
			decodeHandle,
			Jobs::ThreadAffinity::MainThread
		));

		return ref;
	}

	/**
	 * @brief Reloads an asset using the params and mode of its last load.
	 *
	 * Returns false if there are no params to reload from (never loaded, or
	 * unloaded). Returns true once the reload has started. Check status() for
	 * the outcome: a sync reload is already finished, an async one is Pending.
	 */
	template <typename AssetType>
	bool reload(AssetRef<AssetType> ref) {
		auto* store = findStore<AssetType>();

		if (!store)
			return false;

		const auto* slot = store->find(ref.name);

		if (!slot || !slot->params)
			return false;

		const bool wasAsync = slot->async;
		auto params = slot->params->clone();

		unload(ref);

		if (wasAsync)
			loadAsync(ref, *params);
		else
			load(ref, *params);

		return true;
	}

	// ---- Pumping ------------------------------------------------------------

	/**
	 * @brief Promotes completed decode jobs into loaded assets.
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

	/// Outstanding async loads (decode and upload jobs not yet complete).
	size_t pendingCount() const;

	/// Blocks until all outstanding async loads complete.
	void shutdown();

	// ---- Loaders and resolver -----------------------------------------------

	/**
	 * @brief Registers the loaders for asset type AssetType.
	 *
	 * Flushes outstanding async loads first. Replacing a loader while its jobs
	 * are in flight would leave them calling a destroyed object.
	 */
	template <typename AssetType>
	void registerLoader(
		std::unique_ptr<IAssetLoader<AssetType>> sync,
		std::unique_ptr<IAsyncAssetLoader<AssetType>> async = nullptr
	) {
		flushAllPendingUploads();

		auto& store = getStore<AssetType>();

		if (sync)
			sync->setResolver(&assetResolver);

		if (async)
			async->setResolver(&assetResolver);

		store.setSyncLoader(std::move(sync));
		store.setAsyncLoader(std::move(async));
	}

	/// The resolver used for every asset read, in both build modes.
	AssetResolver& resolver() { return assetResolver; }
	const AssetResolver& resolver() const { return assetResolver; }

#ifdef BT_PACK_MODE
	/**
	 * @brief Mounts a .btp pack file onto the resolver stack.
	 *
	 * The last pack mounted wins any ID conflict, so mod packs take priority
	 * over base game packs.
	 */
	bool mountPack(const std::filesystem::path& path) {
		return assetResolver.mount(path);
	}

	/// Assets already loaded from this pack remain alive in their stores.
	void unmountPack(const std::filesystem::path& path) {
		assetResolver.unmount(path);
	}

	std::optional<PackMetadata> getPackMetadata(const std::filesystem::path& path) const {
		return assetResolver.getPackMetadata(path);
	}
#endif

private:
	template <typename AssetType>
	static void requireNamed(AssetRef<AssetType> ref, const char* op) {
		if (ref.isNull())
			throw std::invalid_argument(std::string("AssetManager: null asset ref passed to ") + op);
	}

	template <typename AssetType>
	Store<AssetType>& getStore() {
		const auto key = Detail::typeKey<AssetType>();

		if (auto it = stores.find(key); it != stores.end())
			return *static_cast<Store<AssetType>*>(it->second.get());

		auto store = std::make_unique<Store<AssetType>>();
		auto* result = store.get();

		stores.emplace(key, std::move(store));
		return *result;
	}

	template <typename AssetType>
	Store<AssetType>* findStore() {
		const auto it = stores.find(Detail::typeKey<AssetType>());

		return it != stores.end()
			? static_cast<Store<AssetType>*>(it->second.get())
			: nullptr;
	}

	template <typename AssetType>
	const Store<AssetType>* findStore() const {
		const auto it = stores.find(Detail::typeKey<AssetType>());

		return it != stores.end()
			? static_cast<const Store<AssetType>*>(it->second.get())
			: nullptr;
	}

	NameTable names;

	AssetResolver assetResolver;

	std::unordered_map<const void*, std::unique_ptr<IStore>> stores;

	Jobs::JobSystem& jobs;

	std::atomic<size_t> pendingTotal { 0 };
};

} // namespace Blackthorn::Assets

#include "Assets/AssetHandle.inl"