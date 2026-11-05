#pragma once

#include <stdexcept>
#include <string>
#include <utility>

#include <lua.h>

namespace Blackthorn::Scripting {

/**
 * @brief Creates a subsystem table in the environment and fills it.
 *
 * Call from inside LuaScript::registerApi, where the environment table
 * is on top of the stack. @p fill receives the module table on top of
 * the stack and registers its functions into it, for example with
 * registerFunction().
 *
 * @throws std::logic_error if a module with this name already exists.
 *         Throwing during setup discards the state, so the stack is not repaired.
 */
template <typename Fill>
void registerModule(lua_State* L, const char* name, Fill&& fill) {
	lua_pushstring(L, name);
	lua_rawget(L, -2);
	const bool exists = !lua_isnil(L, -1);
	lua_pop(L, 1);

	if (exists)
		throw std::logic_error("module '" + std::string(name) + "' already registered");

	lua_newtable(L);
	std::forward<Fill>(fill)(L);
	lua_setfield(L, -2, name);
}

} // namespace Blackthorn::Scripting