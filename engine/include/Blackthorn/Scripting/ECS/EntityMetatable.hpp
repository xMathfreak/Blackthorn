#pragma once

#include <lua.h>

namespace Blackthorn::Scripting::ECS {

class ComponentBindings;

/**
 * @brief Creates the entity and component metatables in @p L.
 *
 * Call once per state, from inside LuaScript::registerApi. @p bindings is
 * captured by pointer and must outlive the state.
 */
void installEntityMetatables(lua_State* L, ComponentBindings& bindings);

} // namespace Blackthorn::Scripting::ECS