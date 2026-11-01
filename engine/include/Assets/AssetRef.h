#pragma once

#include <type_traits>

#include "Types/Numeric.h"

namespace Blackthorn::Assets {

/**
 * @brief Typed, POD reference to an asset by its interned ID.
 *
 * Holds no strings or pointers, making it trivially copyable
 * and can sit in components or Lua userdata with no destructor.
 * The value is an index into the manager's name table. Resolution
 * goes through AssetManager, so a ref whose asset is unloaded
 * resolves to nullptr rather than dangling.
 */
template <typename AssetType>
struct AssetRef {
	U32 name = U32_MAX;

	[[nodiscard]]
	constexpr bool isValid() const noexcept {
		return name != U32_MAX;
	}

	[[nodiscard]]
	constexpr bool isNull() const noexcept {
		return name == U32_MAX;
	}


	constexpr explicit operator bool() const noexcept {
		return isValid();
	}

	friend bool operator==(AssetRef, AssetRef) = default;
};

static_assert(std::is_trivially_copyable_v<AssetRef<int>>);
static_assert(std::is_standard_layout_v<AssetRef<int>>);

} // namespace Blackthorn::Assets