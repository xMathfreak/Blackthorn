#include "Scripting/ECS/ScriptWorld.h"

#include <stdexcept>

#include "ECS/Components/Name.h"
#include "ECS/Components/Tag.h"
#include "ECS/Detail.h"
#include "ECS/Queries.h"
#include "Scripting/ECS/ComponentBindings.h"

namespace Blackthorn::Scripting::ECS {

namespace {

using Blackthorn::ECS::Entity;
using Blackthorn::ECS::INVALID_ENTITY;

} // namespace

ScriptWorld::ScriptWorld(Blackthorn::ECS::EntityPool& entPool, ComponentBindings& bind, U32 maxSpawns)
	: pool(entPool)
	, bindings(bind)
	, maxSpawnsPerFrame(maxSpawns)
	, pendingBySlot(pool.getEntities().size(), INVALID_ENTITY)
{}

void ScriptWorld::registerArchetype(std::string name, ArchetypeFn fn) {
	if (!archetypes.try_emplace(std::move(name), std::move(fn)).second)
		throw std::logic_error("duplicate archetype name");
}

EntityHandle ScriptWorld::spawnArchetype(std::string_view archetype) {
	const auto it = archetypes.find(archetype);
	if (it == archetypes.end())
		throw std::runtime_error("unknown archetype '" + std::string(archetype) + "'");

	requireSpawnBudget();

	const Entity entity = pool.create();

	// Undo the spawn if the archetype fails partway.
	try {
		it->second(pool, entity);
	} catch (...) {
		pool.destroy(entity);
		throw;
	}

	++spawnsThisFrame;
	return EntityHandle{ entity };
}

EntityHandle ScriptWorld::spawnEntity() {
	requireSpawnBudget();

	const Entity entity = pool.create();
	++spawnsThisFrame;
	return EntityHandle{entity};
}

bool ScriptWorld::isAlive(EntityHandle handle) const {
	if (!pool.isValid(handle.value))
		return false;

	const auto slot = Blackthorn::ECS::Detail::entityIndex(handle.value);
	return pendingBySlot[slot] != handle.value;
}

void ScriptWorld::requireAlive(EntityHandle handle) const {
	if (!isAlive(handle))
		throw std::runtime_error("stale entity handle");
}

void ScriptWorld::requireSpawnBudget() const {
	if (spawnsThisFrame > maxSpawnsPerFrame)
		throw std::runtime_error("spawn budget for this frame exhausted");
}

const ComponentRecord& ScriptWorld::requireRecord(std::string_view component) const {
	const ComponentRecord* record = bindings.findComponent(component);
	if (!record)
		throw std::runtime_error("unknown component '" + std::string(component) + "'");

	return *record;
}

void ScriptWorld::destroy(EntityHandle handle) {
	requireAlive(handle);

	pendingDestroy.push_back(handle.value);
	pendingBySlot[Blackthorn::ECS::Detail::entityIndex(handle.value)] = handle.value;
}

void ScriptWorld::flushDestroyed() {
	for (const Entity entity : pendingDestroy) {
		// No-op if the engine already destroyed this entity and the slot was
		// reused. isValid checks the generation, so the new occupant is safe.
		pool.destroy(entity);

		const auto slot = Blackthorn::ECS::Detail::entityIndex(entity);
		if (pendingBySlot[slot] == entity)
			pendingBySlot[slot] = INVALID_ENTITY;
	}
	pendingDestroy.clear();
}

const ComponentRecord& ScriptWorld::attachComponent(EntityHandle handle, std::string_view component) {
	requireAlive(handle);

	const ComponentRecord& record = requireRecord(component);
	if (record.has(pool, handle.value))
		throw std::runtime_error("entity already has component '" + record.scriptName + "'");

	record.add(pool, handle.value);
	return record;
}

void ScriptWorld::removeComponent(EntityHandle handle, std::string_view component) {
	requireAlive(handle);

	const ComponentRecord& record = requireRecord(component);
	if (record.has(pool, handle.value))
		throw std::runtime_error("entity already has component '" + record.scriptName + "'");

	record.remove(pool, handle.value);
}

bool ScriptWorld::hasComponent(EntityHandle handle, std::string_view component) {
	requireAlive(handle);
	return requireRecord(component).has(pool, handle.value);
}

std::optional<EntityHandle> ScriptWorld::findEntityNamed(std::string_view name) const {
	const auto found = pool.findEntity<Blackthorn::ECS::Components::Name>(
		[name](const Blackthorn::ECS::Components::Name& n) { return n.value == name; }
	);

	if (!found)
		return std::nullopt;

	const EntityHandle handle{*found};
	if (!isAlive(handle))
		return std::nullopt;

	return handle;
}

void ScriptWorld::addTag(EntityHandle handle, std::string_view tag) {
	requireAlive(handle);

	Blackthorn::ECS::Components::Tag* tags = pool.getComponent<Blackthorn::ECS::Components::Tag>(handle.value);
	if (!tags)
		tags = &pool.addComponent<Blackthorn::ECS::Components::Tag>(handle.value);

	if (!tags->add(tag))
		throw std::runtime_error("entity already has tag '" + std::string(tag) + "'");
}

void ScriptWorld::removeTag(EntityHandle handle, std::string_view tag) {
	requireAlive(handle);

	Blackthorn::ECS::Components::Tag* tags = pool.getComponent<Blackthorn::ECS::Components::Tag>(handle.value);
	if (!tags || !tags->remove(tag))
		throw std::runtime_error("entity does not have tag '" + std::string(tag) + "'");

	if (tags->values.empty())
		pool.removeComponent<Blackthorn::ECS::Components::Tag>(handle.value);   // invalidates `tags`; not used after
}

bool ScriptWorld::hasTag(EntityHandle handle, std::string_view tag) const {
	requireAlive(handle);

	const Blackthorn::ECS::Components::Tag* tags = pool.getComponent<Blackthorn::ECS::Components::Tag>(handle.value);
	return tags && tags->has(tag);
}

std::vector<EntityHandle> ScriptWorld::findEntitiesWithTag(std::string_view tag) const {
	std::vector<EntityHandle> out;

	// The pool doesn't know about pending destruction, so filter here.
	for (const Blackthorn::ECS::Entity entity : Blackthorn::ECS::findEntitiesWithTag(pool, tag)) {
		const EntityHandle handle{ entity };
		if (isAlive(handle))
			out.push_back(handle);
	}
	return out;
}

} // namespace Blackthorn::Scripting