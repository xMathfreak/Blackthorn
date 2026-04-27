#pragma once

#include <cassert>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL.h>

#include "Core/Export.h"
#include "ECS/Entity.h"
#include "Net/Core/ByteBuffer.h"

namespace Blackthorn::ECS {

/**
 * @brief Network entity ID type.
 *
 * A stable, explicitly assigned identifier that is meaningful across
 * machines.
 * Distinct from the local ECS `Entity` handle, which is a
 * machine-local index + generation value.
 */
using NetworkEntityId = Uint64;

static constexpr NetworkEntityId INVALID_NET_ENTITY =
	std::numeric_limits<Uint64>::max();

/**
 * @brief Reason codes carried in @c DespawnPacket payloads.
 *
 * The server writes one of these values when notifying clients that a
 * network entity has been destroyed.
 *
 * Clients can use the reason to play different visuals or audio effects.
 *
 * Values are stable on the wire. Never renumber existing entries.
 */
enum DespawnReason : Uint8 {
	Unknown = 0x00, ///< Unspecified reason.
	Disconnect = 0x01, ///< The owning peer disconnected.
	Death = 0x02, ///< The entity was destroyed by game logic.
	LevelUnload = 0x03 ///< The scene or level containing the entity is being unloaded.
};

/**
 * @brief Bidirectional mapping between NetworkEntityId and local Entity.
 *
 * This registry maintains the relationship between network-level entity
 * identifiers and local ECS entities.
 *
 * @section lookup_performance Lookup Performance
 *
 * - @b netId → local (@c toLocal()):
 *   O(1) lookup via a dense array indexed by NetworkEntityId.
 *   This is the hot path used when applying incoming snapshots, and is
 *   executed for every replicated entity each tick.
 *
 * - @b local → netId (@c toNetId()):
 *   O(1) average lookup via std::unordered_map.
 *   This is the cold path used during snapshot serialization.
 *
 * @section ownership Lifetime and Ownership
 *
 * The registry does not own entities; ownership resides in EntityPool.
 * When an entity is destroyed, the caller must explicitly call remove().
 *
 * The netId → local table may contain INVALID_ENTITY entries for removed or
 * unmapped network IDs. These entries allow stale mappings to be detected
 * without immediate compaction.
 *
 * @section assignment ID Assignment Policy
 *
 * Only the server is authoritative for NetworkEntityId assignment.
 *
 * - Server:
 *   Calls assign(), generating a new NetworkEntityId and establishing the
 *   mapping in a single operation.
 *
 * - Client:
 *   Calls map() when receiving a spawn instruction from the server that
 *   includes both the NetworkEntityId and the locally created Entity.
 *
 * @section thread_safety Thread Safety
 *
 * @note This class is not thread-safe.
 * All write operations must occur on the main thread.
 *
 * Read operations (toLocal(), toNetId(), isMapped()) are safe only
 * if no concurrent mutation is occurring.
 */
class BLACKTHORN_API NetworkEntityRegistry {
public:
	/**
	 * @brief Constructs a registry with an initial ID vector capacity.
	 * @param initialCapacity Pre-allocates the `toLocal` vector to avoid
	 *        early reallocations. Defaults to 256, which is sufficient for
	 *        most game sessions without resizing.
	 */
	explicit NetworkEntityRegistry(size_t initialCapacity = 256) {
		netToLocal.reserve(initialCapacity);
		localToNet.reserve(initialCapacity);
	}

	NetworkEntityRegistry(const NetworkEntityRegistry&) = delete;
	NetworkEntityRegistry& operator=(const NetworkEntityRegistry&) = delete;

	NetworkEntityRegistry(NetworkEntityRegistry&&) = default;
	NetworkEntityRegistry& operator=(NetworkEntityRegistry&&) = default;

	/**
	 * @brief Assigns the next sequential `NetworkEntityId` to `entity` and
	 * stores the mapping. Server-side only.
	 *
	 * The returned ID is guaranteed to be unique for the lifetime of this
	 * registry instance. IDs are never reused after `remove()`.
	 *
	 * @param entity A valid local entity. Must not already have a mapping.
	 * @return The newly assigned `NetworkEntityId`.
	 * @throws std::invalid_argument if `entity` is already mapped.
	 */
	NetworkEntityId assign(Entity entity) {
		if (localToNet.count(entity))
			throw std::invalid_argument(
				"NetworkEntityRegistry::assign: entity already has a network ID"
			);

		const NetworkEntityId netId = nextId++;
		map(netId, entity);
		return netId;
	}

	/**
	 * @brief Records a mapping received from the server in a spawn message.
	 * Client-side only.
	 *
	 * Grows `netToLocal` if `netId` exceeds the current vector size.
	 *
	 * @param netId  The `NetworkEntityId` assigned by the server.
	 * @param entity The local entity created to represent it on this client.
	 * @throws std::invalid_argument if `netId` is already mapped to a
	 *         different valid entity, or if `entity` already has a mapping.
	 */
	void map(NetworkEntityId netId, Entity entity) {
		if (netId == INVALID_NET_ENTITY)
			throw std::invalid_argument(
				"NetworkEntityRegistry::map: cannot map INVALID_NET_ENTITY"
			);

		if (netId < netToLocal.size()) {
			const Entity existing = netToLocal[netId];
			if (existing != INVALID_ENTITY && existing != entity)
				throw std::invalid_argument(
					"NetworkEntityRegistry::map: netId already mapped to a different entity"
				);
		}

		if (localToNet.count(entity)) {
			const NetworkEntityId existingNet = localToNet.at(entity);
			if (existingNet != netId)
				throw std::invalid_argument(
					"NetworkEntityRegistry::map: entity already mapped to a different netId"
				);
			return;
		}

		if (netId >= netToLocal.size())
			netToLocal.resize(netId + 1, INVALID_ENTITY);

		netToLocal[netId] = entity;
		localToNet[entity] = netId;
	}

	/**
	 * @brief Removes the mapping for `entity` on both sides.
	 *
	 * After this call, `toLocal(netId)` returns `INVALID_ENTITY` and
	 * `toNetId(entity)` returns `INVALID_NET_ENTITY`. Safe to call if
	 * the entity is not currently mapped (no-op).
	 *
	 * @param entity The local entity being destroyed or unmapped.
	 */
	void remove(Entity entity) {
		auto it = localToNet.find(entity);
		if (it == localToNet.end())
			return;

		const NetworkEntityId netId = it->second;
		localToNet.erase(it);

		if (netId < netToLocal.size())
			netToLocal[netId] = INVALID_ENTITY;
	}

	/**
	 * @brief Removes the mapping for `netId` on both sides.
	 *
	 * Safe to call if `netId` is not currently mapped (no-op).
	 *
	 * @param netId The network entity ID being unmapped.
	 */
	void removeByNetId(NetworkEntityId netId) {
		if (netId >= netToLocal.size())
			return;

		Entity entity = netToLocal[netId];
		if (entity == INVALID_ENTITY)
			return;

		netToLocal[netId] = INVALID_ENTITY;
		localToNet.erase(entity);
	}

	/**
	 * @brief Clears all mappings and resets the ID counter.
	 *
	 * Call at session end or when loading a new scene that resets all
	 * network state.
	 */
	void clear() {
		netToLocal.clear();
		localToNet.clear();
		nextId = 0;
	}

	/**
	 * @brief Returns the local `Entity` for `netId`.
	 *
	 * Hot path - O(1) flat array lookup. Called for every entity in every
	 * received snapshot.
	 *
	 * @param netId The network entity ID to look up.
	 * @return The local entity, or `INVALID_ENTITY` if not mapped.
	 */
	Entity toLocal(NetworkEntityId netId) const noexcept {
		if (netId >= netToLocal.size())
			return INVALID_ENTITY;
		return netToLocal[netId];
	}

	/**
	 * @brief Returns the `NetworkEntityId` for a local `entity`.
	 *
	 * Cold path - O(1) average hash map lookup. Called only when building
	 * outgoing snapshots, not on the receive path.
	 *
	 * @param entity The local entity to look up.
	 * @return The network ID, or `INVALID_NET_ENTITY` if not mapped.
	 */
	NetworkEntityId toNetId(Entity entity) const noexcept {
		auto it = localToNet.find(entity);
		return it != localToNet.end() ? it->second : INVALID_NET_ENTITY;
	}

	/**
	 * @brief Returns true if `netId` is currently mapped to a valid local entity.
	 */
	bool isMapped(NetworkEntityId netId) const noexcept {
		return netId < netToLocal.size()
			&& netToLocal[netId] != INVALID_ENTITY;
	}

	/**
	 * @brief Returns true if `entity` is currently mapped to a network ID.
	 */
	bool isMapped(Entity entity) const noexcept {
		return localToNet.count(entity) > 0;
	}

	/** @brief Returns the total number of active mappings. */
	size_t mappingCount() const noexcept {
		return localToNet.size();
	}

	/**
	 * @brief Returns the next ID that would be assigned by `assign()`.
	 *
	 * Useful for server-side diagnostics and for embedding the current
	 * ID counter in session save state.
	 */
	NetworkEntityId nextAssignedId() const noexcept {
		return nextId;
	}

	/**
	 * @brief Restores the ID counter after loading session state.
	 *
	 * Should be called before any `assign()` calls when resuming a saved
	 * session so that newly assigned IDs do not collide with existing ones.
	 *
	 * @param id The value the counter should resume from.
	 */
	void restoreNextId(NetworkEntityId id) noexcept {
		nextId = id;
	}

	void serializeSpawn(
		Net::Core::ByteBuffer& buf,
		NetworkEntityId netId,
		Uint32 tick = 0
	) const;

	void serializeDespawn(
		Net::Core::ByteBuffer& buf,
		NetworkEntityId netId,
		DespawnReason reason = DespawnReason::Unknown,
		Uint32 tick = 0
	) const;

private:
	/// Hot path: flat array indexed by NetworkEntityId.
	/// Slots for unmapped or destroyed IDs hold INVALID_ENTITY.
	std::vector<Entity> netToLocal;

	/// Cold path: hash map from local Entity to NetworkEntityId.
	std::unordered_map<Entity, NetworkEntityId> localToNet;

	/// Monotonically increasing counter used by assign(). Never decremented.
	NetworkEntityId nextId = 0;
};

} // namespace Blackthorn::ECS