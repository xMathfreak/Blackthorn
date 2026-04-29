#pragma once

#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "Core/Export.h"
#include "ECS/Entity.h"
#include "Saves/SaveEntityId.h"

namespace Blackthorn::Saves {

/**
 * @brief Bidirectional mapping between @c SaveEntityId and local @c ECS::Entity.
 *
 * Mirrors the structure of @c ECS::NetworkEntityRegistry but with different
 * lifecycle semantics: @c SaveEntityId values are persisted as part of the
 * save file and must survive across sessions.
 *
 * @par ID assignment
 * Only one authority assigns IDs - typically the server for multiplayer or
 * the engine itself for single-player. Call @c assign() when a persistent
 * entity is first created. Call @c map() when loading a saved entity back
 * into a newly created local @c Entity.
 *
 * @par Thread safety
 * Not thread-safe. All operations must occur on the simulation thread.
 *
 * @par Persistence
 * The registry's mapping table and @c nextId counter are serialized by
 * @c WorldSaveSection as part of the @c bt.world section. On load, @c map()
 * is called for each entry and @c restoreNextId() is called to resume
 * monotonic ID assignment from the correct value.
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
	 * @brief Assigns the next sequential @c SaveEntityId to @p entity.
	 *
	 * Call this when a persistent entity is first created and does not yet
	 * have a save identity.
	 *
	 * @param entity A valid local entity. Must not already be mapped.
	 * @return The newly assigned @c SaveEntityId.
	 * @throws std::invalid_argument if @p entity is already mapped.
	 */
	SaveEntityId assign(ECS::Entity entity) {
		if (localToNet.count(entity))
			throw std::invalid_argument("SaveEntityRegistry::assign: Entity already has a save ID");

		const SaveEntityId id = nextId++;
		map(id, entity);
		return id;
	}

	/**
	 * @brief Records a mapping loaded from a save document.
	 *
	 * Call this during save loading after recreating the local entity.
	 *
	 * @param saveId The @c SaveEntityId read from the save document.
	 * @param entity The freshly created local entity.
	 * @throws std::invalid_argument on conflicting mappings.
	 */
	void map(SaveEntityId saveId, ECS::Entity entity) {
		if (saveId == INVALID_SAVE_ENTITY)
			throw std::invalid_argument("SaveEntityRegistry::map: cannot map INVALID_SAVE_ENTITY");

		if (saveId < netToLocal.size()) {
			const ECS::Entity existing = netToLocal[saveId];
			if (existing != ECS::INVALID_ENTITY && existing != entity)
				throw std::invalid_argument("SaveEntityRegistry::map: saveId already mapped to a different entity");
		}

		if (localToNet.count(entity)) {
			if (localToNet.at(entity) != saveId)
				throw std::invalid_argument("SaveEntityRegistry::map: entity already mapped to a different saveId");

			return;
		}

		if (saveId >= netToLocal.size())
			netToLocal.resize(saveId + 1, ECS::INVALID_ENTITY);

		netToLocal[saveId] = entity;
		localToNet[entity] = saveId;
	}


	/**
	 * @brief Removes the mapping for @p entity from both directions.
	 * Safe to call if the entity is not currently mapped.
	 */
	void remove(ECS::Entity entity) {
		auto it = localToNet.find(entity);
		if (it == localToNet.end())
			return;

		const SaveEntityId id = it->second;
		localToNet.erase(it);

		if (id < netToLocal.size())
			netToLocal[id] = ECS::INVALID_ENTITY;
	}

	/**
	 * @brief Removes the mapping for @p saveId from both directions.
	 */
	void removeBySaveId(SaveEntityId saveId) {
		if (saveId >= netToLocal.size())
			return;

		ECS::Entity entity = netToLocal[saveId];
		if (entity == ECS::INVALID_ENTITY)
			return;

		netToLocal[saveId] = ECS::INVALID_ENTITY;
		localToNet.erase(entity);
	}

	void clear() {
		netToLocal.clear();
		localToNet.clear();
		nextId = 0;
	}

	/**
	 * @brief Returns the local entity for @p saveId.
	 */
	ECS::Entity toLocal(SaveEntityId saveId) const noexcept {
		if (saveId >= netToLocal.size())
			return ECS::INVALID_ENTITY;

		return netToLocal[saveId];
	}

	/**
	 * @brief Returns the @c SaveEntityId for @p entity.
	 */
	SaveEntityId toSaveId(ECS::Entity entity) const noexcept {
		auto it = localToNet.find(entity);
		return it != localToNet.end() ? it->second : INVALID_SAVE_ENTITY;
	}

	bool isMapped(SaveEntityId saveId) const noexcept {
		return saveId < netToLocal.size()
			&& netToLocal[saveId] != ECS::INVALID_ENTITY;
	}

	bool isMapped(ECS::Entity entity) const noexcept {
		return localToNet.count(entity) > 0;
	}

	size_t mappingCount() const noexcept {
		return localToNet.size();
	}

	SaveEntityId nextAssignedId() const noexcept {
		return nextId;
	}

	/**
	 * @brief Restores the ID counter after loading from a save file.
	 * Mustbe called before any subsequent @c assign() calls.
	 */
	void restoreNextId(SaveEntityId id) noexcept {
		nextId = id;
	}

	// Iteration support for WorldSaveSection serialization.
	const std::vector<ECS::Entity>& localEntities() const noexcept {
		return netToLocal;
	}

private:
	std::vector<ECS::Entity> netToLocal;
	std::unordered_map<ECS::Entity, SaveEntityId> localToNet;
	SaveEntityId nextId = 0;

};

} // namespace Blackthorn::Saves