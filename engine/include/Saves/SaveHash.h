#pragma once

#include <string_view>

#include "Core/Types/Numeric.h"

namespace Blackthorn::Saves {

/**
 * @brief FNV-1a 64-bit hash for section and entity name identifiers.
 *
 * Used to produce compact numeric IDs from human-readable strings. All
 * section IDs and persistent entity name IDs are stored on disk as their
 * hashed form. The original strings are retained in debug builds for
 * diagnostics but are never written to disk.
 */

namespace Detail {
	static constexpr U64 FNV_OFFSET_BASIS = 14695981039346656037ULL;
	static constexpr U64 FNV_PRIME = 1099511628211ULL;
} // namespace Detail

/**
 * @brief Computes the FNV-1a 64-bit hash of a string at compile time or runtime.
 * @param str Input string.
 * @return 64-bit hash value.
 */
[[nodiscard]]
constexpr U64 saveHash(std::string_view str) noexcept {
	U64 hash = Detail::FNV_OFFSET_BASIS;

	for (unsigned char c : str) {
		hash ^= static_cast<U64>(c);
		hash *= Detail::FNV_PRIME;
	}

	return hash;
}

} // namespace Blackthorn::Saves

/**
 * @brief Compile-time save hash literal.
 *
 * Allows section IDs and entity name IDs to be written as string literals
 * that collapse to their hash at compile time with no runtime cost:
 *
 * @code
 * constexpr U64 id = "bt.world"_saveid;
 * @endcode
 */
[[nodiscard]]
constexpr U64 operator"" _saveid(const char* str, size_t len) noexcept {
	return Blackthorn::Saves::saveHash(std::string_view(str, len));
}