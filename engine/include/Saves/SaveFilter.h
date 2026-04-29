#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Core/Export.h"
#include "Core/Types/Types.h"
#include "Saves/SaveId.h"

namespace Blackthorn::Saves {

/**
 * @brief Optional filter criteria for @c ISaveStorage::list().
 *
 * All fields are optional. Only saves matching every specified criterion
 * are returned. Unset fields match any value.
 *
 * @code
 * // List all autosaves for player "player1" in world "overworld"
 * SaveFilter f;
 * f.worldId  = "overworld";
 * f.playerId = "player1";
 * f.flags    = SaveFlags::Autosave;
 * auto saves = storage.list(f);
 * @endcode
 */
struct BLACKTHORN_API SaveFilter {
	/// If set, only saves with this worldId are returned.
	std::optional<std::string> worldId;

	/// If set, only saves with this playerId are returned.
	std::optional<std::string> playerId;

	/// If set, only saves with this slot index are returned.
	std::optional<U32> slot;

	/// If set, only saves whose flags contain ALL of these bits are returned.
	std::optional<SaveFlags> flags;

	/// If set, only saves created at or after this unix ms timestamp.
	std::optional<U64> createdAfter;

	/// If set, only saves created at or before this unix ms timestamp.
	std::optional<U64> createdBefore;

	/** @brief Returns true if @p id passes all set filter criteria. */
	bool matches(const SaveId& id) const noexcept;

	/** @brief Returns an empty filter that matches everything. */
	static SaveFilter all() { return {}; }
};

/**
 * @brief Lightweight save descriptor returned by @c ISaveStorage::list().
 *
 * Contains the full @c SaveId plus the section ID hashes present in the
 * save, which lets callers check compatibility without decrypting payload data.
 */
struct BLACKTHORN_API SaveMetadata {
	SaveId saveId;

	/// Section ID hashes present in this save's section table.
	/// Used to check whether a save contains expected sections before loading.
	std::vector<U64> sectionIds;

	/// Engine format version recorded in the file header.
	U16 formatVersion = 0;
};

} // namespace Blackthorn::Saves