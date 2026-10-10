#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace Blackthorn::Assets {

class AssetManager;

/**
 * @brief Lifecycle state of an asset as seen through a handle.
 *
 * - Missing: never requested, unloaded, or requested with a different type.
 * - Pending: an async load is in flight.
 * - Ready:   the asset is stored and get() returns a valid pointer.
 * - Failed:  the most recent load attempt failed. Load or reload again to retry.
 */
enum class AssetStatus : uint8_t {
	Missing,
	Pending,
	Ready,
	Failed
};

/**
 * @class AssetHandle
 * @brief A reference to an asset by ID and type. Stores no pointers or job handles.
 *
 * Every query asks the AssetManager for the current state, so a handle obtained
 * while an asset is pending reports Ready once the upload has run. Handles are
 * cheap to copy and can be held across frames.
 *
 * Handles are main-thread only, like AssetManager.
 *
 * @code
 * auto particle = manager.get<ParticleEffect>("smoke");
 * if (particle) {
 *     particles.spawn(*particle, pos);
 * }
 * @endcode
 */
template <typename AssetType>
class AssetHandle {
public:
	AssetHandle() = default;

	AssetHandle(std::string assetID, AssetManager* mgr)
		: id(std::move(assetID))
		, manager(mgr)
	{}

	/// Current status. Missing if the handle is default-constructed.
	AssetStatus status() const;

	bool isReady() const { return status() == AssetStatus::Ready; }
	bool isPending() const { return status() == AssetStatus::Pending; }
	bool isFailed() const { return status() == AssetStatus::Failed; }

	/// True if the handle refers to a manager and a non-empty ID. Says nothing about load state.
	bool isValid() const { return !id.empty() && manager != nullptr; }

	/**
	 * @brief Pointer to the asset, or nullptr unless the status is Ready.
	 *
	 * The pointer is valid until the asset is unloaded or reloaded. Hold the
	 * handle rather than the pointer across frames.
	 */
	AssetType* get() const;

	const std::string& getID() const { return id; }

	/// Dereferences the asset. Only call when the handle is Ready (check with `if (handle)` first).
	AssetType* operator->() const { return get(); }
	AssetType& operator*() const { return *get(); }

	/// True if the asset is Ready.
	explicit operator bool() const { return isReady(); }

private:
	std::string id;
	AssetManager* manager = nullptr;
};

} // namespace Blackthorn::Assets
