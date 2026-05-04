#pragma once

#include <array>
#include <string>

#include "Core/Export.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::Core {

/**
 * @brief Version-4 UUID stored as 16 raw bytes.
 *
 * No external UUID library is required. Generation is handled by
 * @c SaveId::generate() using libsodium's @c randombytes_buf.
 */
struct BLACKTHORN_API UUID {
	std::array<U8, 16> bytes{};

	bool operator==(const UUID& other) const noexcept { return bytes == other.bytes; }
	bool operator!=(const UUID& other) const noexcept { return bytes != other.bytes; }
	bool operator<(const UUID& other) const noexcept { return bytes < other.bytes; }

	/** @brief Returns true if all bytes are zero (default-constructed). */
	bool isNull() const noexcept;

	/**
	 * @brief Returns the UUID formatted as a lowercase hyphenated string.
	 * e.g. "550e8400-e29b-41d4-a716-446655440000"
	 */
	std::string toString() const;

	/**
	 * @brief Parses a hyphenated UUID string. Returns a null UUID on failure.
	 */
	static UUID fromString(std::string_view str);

	/**
	 * @brief Makes a stable UUID from a compile time string.
	 */
	static UUID makeStable(std::string_view seed);
};

} // namespace Blackthorn::Core

namespace std {

template <>
struct hash<Blackthorn::Core::UUID> {
	size_t operator()(const Blackthorn::Core::UUID& uuid) const noexcept {
		U64 h = 14695981039346656037ULL;
		for (U8 b : uuid.bytes) {
			h ^= static_cast<U64>(b);
			h *= 1099511628211ULL;
		}

		return static_cast<size_t>(h);
	}
};

} // namespace std