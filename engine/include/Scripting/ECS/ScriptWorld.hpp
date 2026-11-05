#pragma once

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "ECS/Entity.h"
#include "ECS/EntityPool.h"
#include "Scripting/ECS/EntityHandle.h"
#include "Types/Numeric.h"

namespace Blackthorn::Scripting::ECS {

class ComponentBindings;
struct ComponentRecord;

/**
 * @brief Adds the components for one archetype to a newly created entity.
 *
 * Runs inside spawn(). If it throws, the entity is destroyed again.
 */
using ArchetypeFn = std::function<void(Blackthorn::ECS::EntityPool&, Blackthorn::ECS::Entity)>;

/**
 * @brief Componenthe engine-side world that entity API functions operate on.
 *
 * Owns the spawn budget, the archetype table, and deferred destruction.
 * Main-thread only. Must not outlive the EntityPool it refers to.
 */
class BLACKTHORN_API ScriptWorld {
public:
	/**
	 * @param entPool   Pool to operate on.
	 * @param bind      Component names and their operations.
	 * @param maxSpawns Spawns allowed between beginFrame() calls.
	 */
	ScriptWorld(Blackthorn::ECS::EntityPool& entPool, ComponentBindings& bind, U32 maxSpawns = 256);

	ScriptWorld(const ScriptWorld&) = delete;
	ScriptWorld& operator=(const ScriptWorld&) = delete;

	/**
	 * @brief Registers a named archetype that scripts can spawn.
	 *
	 * Call during setup only. Duplicate names are rejected.
	 * @throws std::logic_error on a duplicate name.
	 */
	void registerArchetype(std::string name, ArchetypeFn fn);

	/// Resets the spawn budget. Call once at the start of each frame.
	void beginFrame() noexcept { spawnsThisFrame = 0; }

	/**
	 * @brief Creates an entity from a named archetype.
	 * @throws std::runtime_error if the archetype is unknown, the frame's
	 *         spawn budget is spent, or the pool is out of slots.
	 */
	EntityHandle spawnArchetype(std::string_view archetype);

	/**
	 * @brief Creates an entity.
	 * @throws std::runtime_error if the frame's spawn budget is spent
	 *         or the pool is out of slots.
	 */
	EntityHandle spawnEntity();

	/**
	 * @brief Queues an entity for destruction at the next flushDestroyed().
	 * @throws std::runtime_error if the handle is stale.
	 */
	void destroy(EntityHandle handle);

	/// @return true if the handle is live and not queued for destruction.
	[[nodiscard]] bool isAlive(EntityHandle handle) const;

	/**
	 * @brief Applies queued destructions. Call between systems, never inside
	 *        a view iteration.
	 */
	void flushDestroyed();

	/// @throws std::runtime_error if the handle is stale or lacks component
	template <typename Component>
	Component& requireComponent(EntityHandle handle) {
		requireAlive(handle);

		Component* component = pool.getComponent<Component>(handle.value);
		if (!component)
			throw std::runtime_error("entity does not have required component");

		return *component;
	}

	/// @throws std::runtime_error if the handle is stale or lacks component.
	template <typename Component>
	const Component& requireComponent(EntityHandle handle) const {
		requireAlive(handle);

		const Component* component = pool.getComponent<Component>(handle.value);
		if (!component)
			throw std::runtime_error("entity does not have required component");

		return *component;
	}

	/**
	 * @brief Adds a declared component with its default or factory value.
	 * @throws std::runtime_error if the handle is stale, the component is
	 *         unknown or the entity already has it.
	 */
	const ComponentRecord& attachComponent(EntityHandle handle, std::string_view component);

	/**
	 * @brief Removes a declared component.
	 * @throws std::runtime_error if the handle is stale, the component is
	 *         unknown or the entity does not have it.
	 */
	void removeComponent(EntityHandle handle, std::string_view component);

	/**
	 * @throws std::runtime_error if the handle is stale or the component is
	 *         unknown.
	 */
	bool hasComponent(EntityHandle handle, std::string_view component);

	/**
	 * @brief Returns the live entity named @p name, or nullopt if none is.
	 * @note An entity queued for destruction is treated as absent.
	 */
	[[nodiscard]]
	std::optional<EntityHandle> findEntityNamed(std::string_view name) const;

	/// Adds a tag. Creates the Tag component on first use.
	/// @throws std::runtime_error if the handle is stale or the tag is already present.
	void addTag(EntityHandle handle, std::string_view tag);

	/// Removes a tag. Removes the Tag component when no tags remain.
	/// @throws std::runtime_error if the handle is stale or the tag is absent.
	void removeTag(EntityHandle handle, std::string_view tag);

	/// @throws std::runtime_error if the handle is stale.
	[[nodiscard]] bool hasTag(EntityHandle handle, std::string_view tag) const;

	/// Live entities carrying @p tag, in deterministic order.
	[[nodiscard]] std::vector<EntityHandle> findEntitiesWithTag(std::string_view tag) const;

	/// @return The component, or nullptr if the entity lacks it. Throws only if the handle is stale.
	template <typename C>
	const C* tryComponent(EntityHandle handle) const {
		requireAlive(handle);
		return pool.getComponent<C>(handle.value);
	}

private:
	void requireAlive(EntityHandle handle) const;
	void requireSpawnBudget() const;
	const ComponentRecord& requireRecord(std::string_view component) const;

	Blackthorn::ECS::EntityPool& pool;
	ComponentBindings& bindings;
	U32 maxSpawnsPerFrame;
	U32 spawnsThisFrame = 0;

	/// std::less<> enables lookup by std::string_view without allocating.
	std::map<std::string, ArchetypeFn, std::less<>> archetypes;

	std::vector<Blackthorn::ECS::Entity> pendingDestroy;

	/// Slot index -> the entity queued for destruction in that slot, or
	/// INVALID_ENTITY. Keyed by slot and storing the full Entity, so a reused
	/// slot can't be mistaken for the entity that was queued.
	std::vector<Blackthorn::ECS::Entity> pendingBySlot;
};

} // namespace Blackthorn::Scripting::ECS