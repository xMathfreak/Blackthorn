#pragma once

#include <string>

#include "Core/Export.h"
#include "Saves/SaveEntityId.h"

namespace Blackthorn::ECS::Components {

/**
 * @brief Marks an ECS entity as eligible for save serialization.
 *
 * Adding this component to an entity is the only action required to opt it
 * into the save system. The @c SaveManager will include it in the @c bt.world
 * section when writing a save and reconstruct it on load.
 *
 * @par Save identity
 * @c saveId is assigned by @c SaveEntityRegistry::assign() the first time a
 * persistent entity is registered with the save system. It is @c INVALID_SAVE_ENTITY
 * until then. Do not set it manually.
 *
 * @par Stable name
 * @c name is a human-readable identifier used for authoring, debugging, and
 * optional lookup-by-name on load. It is hashed to a @c U64 for on-disk
 * storage. It does not need to be globally unique but should be descriptive
 * enough to be useful in log output (e.g. "player_character", "boss_door_1").
 *
 * @code
 * Entity player = pool.create();
 * pool.addComponent<Components::Persistent>(player, "player_character");
 * // saveManager.registerEntity(player), called by WorldSaveSection
 * @endcode
 */
struct BLACKTHORN_API Persistent {
	/// Human-readable name for authoring and diagnostics.
	std::string name;

	/// Save-scoped identity. Assigned by SaveEntityRegistry. Do not set manually.
	Saves::SaveEntityId saveId = Saves::INVALID_SAVE_ENTITY;

	/// Hashed form of name, computed on first registration. Stored on disk.
	U64 nameHash = 0;

	Persistent() = default;

	explicit Persistent(std::string entityName)
		: name(std::move(entityName))
	{}
};

} // namespace Blackthorn::ECS::Components