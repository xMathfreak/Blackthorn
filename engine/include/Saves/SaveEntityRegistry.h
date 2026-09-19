#pragma once

#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "Core/Export.h"
#include "ECS/Entity.h"
#include "Saves/SaveEntityID.h"

namespace Blackthorn::Saves {


/**
 * @brief Bidirectional mapping between @c SaveEntityID and local @c ECS::Entity.
 *
 * Mirrors the structure of @c ECS::NetworkEntityRegistry, but with different
 * life cycle semantics: @c SaveEntityID values are persistent identifiers
 * stored in save files, while @c ECS::Entity values are recreated when a world
 * is loaded.
 *
 * @par ID assignment
 * A single authority assigns persistent IDs, typically the server in multiplayer
 * or the engine in single player. Call @c assign() when a persistent entity is
 * first created. When loading a saved entity, create its new local @c ECS::Entity
 * and call @c map() to associate it with the saved @c SaveEntityID.
 *
 * @par Thread safety
 * Not thread-safe. All operations must be performed on the simulation thread.
 *
 * @par Persistence
 * The mapping table and @c nextID counter are serialized by @c WorldSaveSection as
 * part of the @c bt.world section. During loading, call map() for each saved mapping
 * and restoreNextID() to resume monotonic ID assignment from the correct value.
 */
class BLACKTHORN_API SaveEntityRegistry {
public:
	explicit SaveEntityRegistry(size_t initialCapacity = 256) {
		netToLocal.reserve(initialCapacity);
		localToNet.reserve(initialCapacity);
	}

	SaveEntityRegistry(const SaveEntityRegistry&) = delete;
	SaveEntityRegistry& operator=(const SaveEntityRegistry&) = delete;

	SaveEntityRegistry(SaveEntityRegistry&&) = default;
	SaveEntityRegistry& operator=(SaveEntityRegistry&&) = default;

	/**
	 * @brief Assigns the next sequential @c SaveEntityID to @p entity.
	 *
	 * Call this when a persistent entity is first created and does not yet
	 * have a save identity.
	 *
	 * @param entity A valid local entity. Must not already be mapped.
	 * @return The newly assigned @c SaveEntityID.
	 * @throws std::invalid_argument if @p entity is already mapped.
	 */
	SaveEntityID assign(ECS::Entity entity) {
		if (localToNet.count(entity))
			throw std::invalid_argument("SaveEntityRegistry::assign: Entity already has a save ID");

		const SaveEntityID id = nextID++;
		map(id, entity);
		return id;
	}

	/**
	 * @brief Records a mapping loaded from a save document.
	 *
	 * Call this during save loading after recreating the local entity.
	 *
	 * @param saveID The @c SaveEntityID read from the save document.
	 * @param entity The freshly created local entity.
	 * @throws std::invalid_argument on conflicting mappings.
	 */
	void map(SaveEntityID saveID, ECS::Entity entity) {
		if (saveID == INVALID_SAVE_ENTITY)
			throw std::invalid_argument("SaveEntityRegistry::map: cannot map INVALID_SAVE_ENTITY");

		if (saveID < netToLocal.size()) {
			const ECS::Entity existing = netToLocal[saveID];
			if (existing != ECS::INVALID_ENTITY && existing != entity)
				throw std::invalid_argument("SaveEntityRegistry::map: saveID already mapped to a different entity");
		}

		if (localToNet.count(entity)) {
			if (localToNet.at(entity) != saveID)
				throw std::invalid_argument("SaveEntityRegistry::map: entity already mapped to a different saveID");

			return;
		}

		if (saveID >= netToLocal.size())
			netToLocal.resize(saveID + 1, ECS::INVALID_ENTITY);

		netToLocal[saveID] = entity;
		localToNet[entity] = saveID;
	}


	/**
	 * @brief Removes the mapping for @p entity from both directions.
	 * Safe to call if the entity is not currently mapped.
	 */
	void remove(ECS::Entity entity) {
		auto it = localToNet.find(entity);
		if (it == localToNet.end())
			return;

		const SaveEntityID id = it->second;
		localToNet.erase(it);

		if (id < netToLocal.size())
			netToLocal[id] = ECS::INVALID_ENTITY;
	}

	/**
	 * @brief Removes the mapping for @p saveID from both directions.
	 */
	void removeBySaveID(SaveEntityID saveID) {
		if (saveID >= netToLocal.size())
			return;

		ECS::Entity entity = netToLocal[saveID];
		if (entity == ECS::INVALID_ENTITY)
			return;

		netToLocal[saveID] = ECS::INVALID_ENTITY;
		localToNet.erase(entity);
	}

	void clear() {
		netToLocal.clear();
		localToNet.clear();
		nextID = 0;
	}

	/**
	 * @brief Returns the local entity for @p saveID.
	 */
	ECS::Entity toLocal(SaveEntityID saveID) const noexcept {
		if (saveID >= netToLocal.size())
			return ECS::INVALID_ENTITY;

		return netToLocal[saveID];
	}

	/**
	 * @brief Returns the @c SaveEntityID for @p entity.
	 */
	SaveEntityID toSaveID(ECS::Entity entity) const noexcept {
		auto it = localToNet.find(entity);
		return it != localToNet.end() ? it->second : INVALID_SAVE_ENTITY;
	}

	bool isMapped(SaveEntityID saveID) const noexcept {
		return saveID < netToLocal.size()
			&& netToLocal[saveID] != ECS::INVALID_ENTITY;
	}

	bool isMapped(ECS::Entity entity) const noexcept {
		return localToNet.count(entity) > 0;
	}

	size_t mappingCount() const noexcept {
		return localToNet.size();
	}

	SaveEntityID nextAssignedID() const noexcept {
		return nextID;
	}

	/**
	 * @brief Restores the ID counter after loading from a save file.
	 * Must be called before any subsequent @c assign() calls.
	 */
	void restoreNextID(SaveEntityID id) noexcept {
		nextID = id;
	}

	// Iteration support for WorldSaveSection serialization.
	const std::vector<ECS::Entity>& localEntities() const noexcept {
		return netToLocal;
	}

private:
	std::vector<ECS::Entity> netToLocal;
	std::unordered_map<ECS::Entity, SaveEntityID> localToNet;
	SaveEntityID nextID = 0;

};

} // namespace Blackthorn::Saves