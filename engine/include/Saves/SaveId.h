#pragma once

#include <string>

#include "Core/Export.h"
#include "Core/Types/Numeric.h"
#include "Core/Types/UUID.h"

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
	Core::UUID id;

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
struct hash<Blackthorn::Saves::SaveId> {
	size_t operator()(const Blackthorn::Saves::SaveId& id) const noexcept {
		return std::hash<Blackthorn::Core::UUID>{}(id.id);
	}
};

} // namespace std