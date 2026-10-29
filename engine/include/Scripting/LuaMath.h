#pragma once

#include "Scripting/LuaConverter.h"

#include <glm/vec2.hpp>

namespace Blackthorn::Scripting {

namespace Detail {

/**
 * @brief Reads a numeric field from a table without invoking metamethods.
 */
inline LuaResult<float> readFloatField(lua_State* L, int tableIndex, const char* key) {
	lua_pushstring(L, key);
	lua_rawget(L, tableIndex);

	LuaResult<double> value = LuaConverter<double>::fromLua(L, -1);
	lua_pop(L, 1);

	if (!value.ok())
		return LuaError{ std::string(key) + ": number expected", 0 };
	return static_cast<float>(value.value());
}

} // namespace Detail

/**
 * @brief Converts { x = number, y = number } to glm::vec2.
 */
template <>
struct LuaConverter<glm::vec2> {
	static LuaResult<glm::vec2> fromLua(lua_State* L, int index) {
		if (lua_type(L, index) != LUA_TTABLE)
			return LuaError{ "vector table expected", index };

		const int table = lua_absindex(L, index);

		LuaResult<float> x = Detail::readFloatField(L, table, "x");
		if (!x.ok())
			return x.error();

		LuaResult<float> y = Detail::readFloatField(L, table, "y");
		if (!y.ok())
			return y.error();

		return glm::vec2{ x.value(), y.value() };
	}

	static void push(lua_State* L, const glm::vec2& value) {
		lua_createtable(L, 0, 2);
		lua_pushnumber(L, value.x);
		lua_setfield(L, -2, "x");
		lua_pushnumber(L, value.y);
		lua_setfield(L, -2, "y");
	}
};

} // namespace Blackthorn::Scripting