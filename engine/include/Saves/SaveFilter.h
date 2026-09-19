#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Core/Export.h"
#include "Core/Types/Numeric.h"
#include "Saves/SaveID.h"

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
 * f.worldID  = "overworld";
 * f.playerID = "player1";
 * f.flags    = SaveFlags::Autosave;
 * auto saves = storage.list(f);
 * @endcode
 */
struct BLACKTHORN_API SaveFilter {
	/// If set, only saves with this worldID are returned.
	std::optional<std::string> worldID;

	/// If set, only saves with this playerID are returned.
	std::optional<std::string> playerID;

	/// If set, only saves with this slot index are returned.
	std::optional<U32> slot;

	/// If set, only saves whose flags contain ALL of these bits are returned.
	std::optional<SaveFlags> flags;

	/// If set, only saves created at or after this unix ms timestamp.
	std::optional<U64> createdAfter;

	/// If set, only saves created at or before this unix ms timestamp.
	std::optional<U64> createdBefore;

	/** @brief Returns true if @p id passes all set filter criteria. */
	bool matches(const SaveID& id) const noexcept;

	/** @brief Returns an empty filter that matches everything. */
	static SaveFilter all() { return {}; }
};

/**
 * @brief Lightweight save descriptor returned by @c ISaveStorage::list().
 *
 * Contains the full @c SaveID plus the section ID hashes present in the
 * save, which lets callers check compatibility without decrypting payload data.
 */
struct BLACKTHORN_API SaveMetadata {
	SaveID saveID;

	/// Section ID hashes present in this save's section table.
	/// Used to check whether a save contains expected sections before loading.
	std::vector<U64> sectionIDs;

	/// Engine format version recorded in the file header.
	U16 formatVersion = 0;
};

} // namespace Blackthorn::Saves