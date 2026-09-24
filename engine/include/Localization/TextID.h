#pragma once

#include <compare> // IWYU pragma: keep
#include <functional>
#include <string_view>

#include "Core/Export.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::Localization {

class TextID {
public:
	constexpr explicit TextID(U64 v)
		: val(v)
	{}

	constexpr U64 value() const { return val; }

	bool operator<=>(const TextID&) const = default;
private:
	U64 val;
};

/**
 * @brief Computes the TextID for a human-readable localization key.
 *
 * @details Comutes a stable, portable TextID from the localization key using
 * 			XXH64 with seed 0. XXH64 is used so the same localization key
 * 			produces the same TextID regardless of platform, compiler and shared
 * 			library. This allows the localization compiler to precompute TextIDs
 *			that match those generated at runtime and ensures that compiled
 * 			.btloc files remain portable across machines.
 *
 * @param id Human-readable localization key, e.g. "inventory.potions_found".
 * @return The TextID used to key entries in a loaded Source.
 */
[[nodiscard]] BLACKTHORN_API TextID makeTextID(std::string_view id) noexcept;

} // namespace Blackthorn::Localization

namespace std {

template<>
struct hash<Blackthorn::Localization::TextID> {
	size_t operator()(const Blackthorn::Localization::TextID& t) const noexcept {
		return static_cast<size_t>(t.value());
	}
};

} // namespace std
