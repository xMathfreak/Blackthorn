#pragma once

#include "Blackthorn/Assets/AssetRef.hpp"
#include "Blackthorn/Assets/AssetStatus.hpp"

namespace Blackthorn::Assets {

class AssetManager;

template <typename AssetType>
class AssetHandle {
public:
	AssetHandle() = default;

	AssetHandle(AssetRef<AssetType> r, AssetManager* mgr)
		: ref(r)
		, manager(mgr)
	{}

	[[nodiscard]] bool isValid() const noexcept {
		return ref.isValid() && manager != nullptr;
	}

	[[nodiscard]] AssetStatus status() const;

	[[nodiscard]] bool isReady() const { return status() == AssetStatus::Ready; }
	[[nodiscard]] bool isPending() const { return status() == AssetStatus::Pending; }
	[[nodiscard]] bool isFailed() const { return status() == AssetStatus::Failed; }

	explicit operator bool() const noexcept { return get() != nullptr; }

	/**
	 * @brief Pointer to the asset or nullptr if the status is not Ready.
	 *
	 * The pointer is valid until the asset is unloaded or reloaded.
	 */
	AssetType* get() const;

	AssetRef<AssetType> getRef() const noexcept {
		return ref;
	}

	/// Dereferences the asset. Only call after a ready check.
	AssetType* operator->() const { return get(); }
	AssetType& operator*() const { return *get(); }

private:
	AssetRef<AssetType> ref{U32_MAX};
	AssetManager* manager = nullptr;
};

} // namespace Blackthorn::Assets