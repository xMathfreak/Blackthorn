#pragma once

#include <array>
#include <string>

#include "Core/Export.h"
#include "Core/Types/Types.h"

namespace Blackthorn::Saves {

/**
 * @brief Bitmask flags carried in @c SaveId::flags.
 */
enum class SaveFlags : U32 {
	None = 0,
	Autosave = 1 << 0, ///< Written automatically by the engine on a schedule.
	Quicksave = 1 << 1, ///< Written on explicit player request.
	Backup = 1 << 2, ///< A backup copy made before an overwrite.
};

inline SaveFlags operator|(SaveFlags a, SaveFlags b) {
	return static_cast<SaveFlags>(
		static_cast<U32>(a) | static_cast<U32>(b)
	);
}

inline SaveFlags operator&(SaveFlags a, SaveFlags b) {
	return static_cast<SaveFlags>(
		static_cast<U32>(a) & static_cast<U32>(b)
	);
}

inline bool hasFlag(SaveFlags flags, SaveFlags flag) {
	return (static_cast<U32>(flags) & static_cast<U32>(flag)) != 0;
}

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

/**
 * @brief Primary identity and metadata for a save file or save slot.
 *
 * The @c id field is the canonical identity - all storage lookups go through
 * it. All other fields are metadata used for display, filtering, and grouping.
 * Storage backends may use @c worldId, @c playerId, and @c slot to construct
 * a directory layout, but the canonical key is always the UUID.
 *
 * @par Generating a new SaveId
 * @code
 * SaveId save = SaveId::generate();
 * save.displayName = "Before the final boss";
 * save.worldId     = "overworld";
 * save.playerId    = "player_1";
 * @endcode
 */
struct BLACKTHORN_API SaveId {
	/// Canonical identity. Never changes after creation.
	UUID id;

	/// Human-readable label shown in the save list UI.
	std::string displayName;

	/// Game-defined world or level identifier. Used for grouping and
	/// storage path construction. Empty means globally scoped.
	std::string worldId;

	/// Game-defined player identifier. Empty means the save is not
	/// player-scoped (e.g. a global world state save).
	std::string playerId;

	/// Optional slot index for games with a fixed number of save slots.
	/// 0 means the save is not slot-bound.
	U32 slot = 0;

	/// Bitmask of @c SaveFlags.
	SaveFlags flags = SaveFlags::None;

	/// Unix timestamp in milliseconds when this save was first created.
	U64 createdAt = 0;

	/// Unix timestamp in milliseconds when this save was last written.
	U64 updatedAt = 0;

	/**
	 * @brief Creates a new SaveId with a freshly generated UUID and
	 * timestamps set to the current time.
	 */
	static SaveId generate();

	bool operator==(const SaveId& other) const noexcept { return id == other.id; }
	bool operator!=(const SaveId& other) const noexcept { return id != other.id; }
};

} // namespace Blackthorn::Saves

namespace std {

template <>
struct hash<Blackthorn::Saves::UUID> {
	size_t operator()(const Blackthorn::Saves::UUID& uuid) const noexcept {
		U64 h = 14695981039346656037ULL;
		for (U8 b : uuid.bytes) {
			h ^= static_cast<U64>(b);
			h *= 1099511628211ULL;
		}

		return static_cast<size_t>(h);
	}
};

template <>
struct hash<Blackthorn::Saves::SaveId> {
	size_t operator()(const Blackthorn::Saves::SaveId& id) const noexcept {
		return std::hash<Blackthorn::Saves::UUID>{}(id.id);
	}
};

} // namespace std