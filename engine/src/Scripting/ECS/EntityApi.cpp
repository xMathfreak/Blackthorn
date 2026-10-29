#include "Scripting/ECS/EntityApi.h"

#include "Scripting/ECS/EntityMetatable.h"
#include "Scripting/Module.h"

namespace Blackthorn::Scripting::ECS {

namespace Detail {

ScriptWorld*& activeWorld() noexcept {
	static thread_local ScriptWorld* world = nullptr;
	return world;
}

ScriptWorld& requireWorld() {
	ScriptWorld* world = activeWorld();
	if (!world)
		throw std::logic_error("entity API called with no world bound");
	return *world;
}

} // namespace Detail

void registerEntityApi(LuaScript& script, ComponentBindings& bindings) {
	script.registerApi([&bindings](lua_State* L) {
		installEntityMetatables(L, bindings);

		registerModule(L, "ecs", [](lua_State* M) {
			registerFunction<&spawnEntity>(M, "spawnEntity");
			registerFunction<&spawnArchetype>(M, "spawnArchetype");
			registerFunction<&destroyEntity>(M, "destroyEntity");
			registerFunction<&isEntityAlive>(M, "isAlive");
			registerFunction<&getEntity>(M, "getEntity");
			registerFunction<&getEntities>(M, "getEntities");
		});
	});
}

} // namespace Blackthorn::Scripting
