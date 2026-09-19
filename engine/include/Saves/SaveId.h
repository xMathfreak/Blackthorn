#pragma once

#include <string>

#include "Core/Export.h"
#include "Core/Types/Numeric.h"
#include "Core/Types/UUID.h"

namespace Blackthorn::Saves {

/**
 * @brief Bitmask flags carried in @c SaveID::flags.
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
 * @brief Canonical identity and metadata for a save file or save slot.
 *
 * @details
 * The @c id field is the canonical identity of the save; all storage lookups
 * use it as the primary key. The remaining fields are metadata used for
 * display, filtering, and grouping. Storage backends may use @c worldID,
 * @c playerID, and @c slot to organize their directory layout, but the
 * canonical storage key is always the UUID in @c id.
 *
 * @par Generating a new SaveID
 * @code
 * SaveID save = SaveID::generate();
 * save.displayName = "Before the final boss";
 * save.worldID     = "overworld";
 * save.playerID    = "player_1";
 * @endcode
 */
struct BLACKTHORN_API SaveID {
	/// Canonical identity. Never changes after creation.
	Core::UUID id;

	/// Human-readable label shown in the save list UI.
	std::string displayName;

	/// Game-defined world or level identifier. Used for grouping and
	/// storage path construction. Empty means globally scoped.
	std::string worldID;

	/// Game-defined player identifier. Empty means the save is not
	/// player-scoped (e.g. a global world state save).
	std::string playerID;

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
	 * @brief Creates a new SaveID with a freshly generated UUID and
	 * timestamps set to the current time.
	 */
	static SaveID generate();

	bool operator==(const SaveID& other) const noexcept { return id == other.id; }
	bool operator!=(const SaveID& other) const noexcept { return id != other.id; }
};

} // namespace Blackthorn::Saves

namespace std {

template <>
struct hash<Blackthorn::Saves::SaveID> {
	size_t operator()(const Blackthorn::Saves::SaveID& id) const noexcept {
		return std::hash<Blackthorn::Core::UUID>{}(id.id);
	}
};

} // namespace std