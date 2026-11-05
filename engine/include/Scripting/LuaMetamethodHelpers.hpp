#pragma once

#include <lua.h>
#include <lauxlib.h>

#include "Scripting/LuaResult.h"

namespace Blackthorn::Scripting {

/// Index of the first real argument: 2 for a colon call (self at 1), 1 for a dot call.
inline int argsOffset(lua_State* L, const char* metatable) {
	return luaL_testudata(L, 1, metatable) ? 2 : 1;
}

template <typename Fn>
int runMetamethod(lua_State* L, Fn&& body) {
	LuaResult<int> result = [&]() -> LuaResult<int> {
		try {
			return body();
		} catch (const std::exception& e) {
			return LuaError{e.what(), 0};
		}
	}();

	if (result.ok())
		return result.value();

	const LuaError& error = result.error();
	lua_pushlstring(L, error.message.data(), error.message.size());
	return lua_error(L);
}

/// A string argument. The view points into the Lua string, which stays valid while it is on the stack.
inline LuaResult<std::string_view> stringArg(lua_State* L, int index) {
	if (lua_type(L, index) != LUA_TSTRING)
		return LuaError{ "name must be a string", index };

	size_t length = 0;
	const char* text = lua_tolstring(L, index, &length);
	return std::string_view(text, length);
}

/// Pushes a method bound to data at index 1, and returns 1.
inline int pushMethod(lua_State* L, lua_CFunction fn) {
	lua_pushvalue(L, 1);
	lua_pushcclosure(L, fn, 1);
	return 1;
}

} // namespace Blackthorn::Scripting
