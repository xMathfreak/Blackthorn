#pragma once

#include <stdexcept>
#include <string>

#include "Core/Export.h"
#include "Scripting/LuaScript.h"
#include "Scripting/ECS/ComponentBindings.h"
#include "Scripting/ECS/ScriptWorld.h"

namespace Blackthorn::Scripting::ECS {

namespace Detail {

/// The world bound to the current thread's script calls, or null.
BLACKTHORN_API ScriptWorld*& activeWorld() noexcept;
ScriptWorld& requireWorld();

} // namespace Detail

/**
 * @brief Binds a ScriptWorld to the current thread for one scope.
 *
 * Nesting restores the previous binding on exit.
 */
class ScriptWorldScope {
public:
	explicit ScriptWorldScope(ScriptWorld& world) noexcept
		: previous(Detail::activeWorld()) {
		Detail::activeWorld() = &world;
	}

	~ScriptWorldScope() {
		Detail::activeWorld() = previous;
	}

	ScriptWorldScope(const ScriptWorldScope&) = delete;
	ScriptWorldScope& operator=(const ScriptWorldScope&) = delete;

private:
	ScriptWorld* previous;
};

// Functions exposed to scripts. Each takes and returns converter-supported types only.

inline EntityHandle spawnArchetype(const std::string& archetype) {
	return Detail::requireWorld().spawnArchetype(archetype);
}

inline EntityHandle spawnEntity() {
	return Detail::requireWorld().spawnEntity();
}

inline void destroyEntity(EntityHandle entity) {
	Detail::requireWorld().destroy(entity);
}

inline bool isEntityAlive(EntityHandle entity) {
	return Detail::requireWorld().isAlive(entity);
}

inline std::optional<EntityHandle> getEntity(const std::string& name) {
	return Detail::requireWorld().findEntityNamed(name);
}

inline std::vector<EntityHandle> getEntities(const std::string& tag) {
	return Detail::requireWorld().findEntitiesWithTag(tag);
}

/**
 * @brief Registers the entity API in a script's environment.
 *
 * Call once during setup. The functions raise a Lua error when no world is
 * bound.
 */
void BLACKTHORN_API registerEntityApi(LuaScript &script, ComponentBindings &bindings);

} // namespace Blackthorn::Scripting