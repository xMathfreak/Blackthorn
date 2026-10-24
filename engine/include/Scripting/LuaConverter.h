#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <lua.h>
#include <lauxlib.h>

#include "Scripting/LuaResult.h"

namespace Blackthorn::Scripting {

/// Primary template. Specialize for each type that crosses the Lua boundary.
template <typename T>
struct LuaConverter;

namespace Detail {

/// Maximum depth of nested tables a single conversion may traverse.
inline constexpr int kMaxNestingDepth = 32;

/// Maximum elements in one converted table. Limits C++-side growth,
/// which the Lua allocator budget does not cover.
inline constexpr size_t kMaxContainerElements = size_t{ 1 } << 20;

/**
 * @brief RAII depth counter for nested conversions on the current thread.
 *
 * Constructed at the start of each container conversion. If the limit is
 * exceeded, ok() returns false and the caller reports an error.
 * Thread-local, which matches LuaScript's thread affinity.
 */
class NestingGuard {
public:
	NestingGuard() : ok_(++depth() <= kMaxNestingDepth) {}
	~NestingGuard() { --depth(); }

	NestingGuard(const NestingGuard&) = delete;
	NestingGuard& operator=(const NestingGuard&) = delete;

	/// @return true if this level is within the nesting limit.
	[[nodiscard]] bool ok() const noexcept { return ok_; }

private:
	static int& depth() noexcept {
		static thread_local int d = 0;
		return d;
	}

	bool ok_;
};

/**
 * @brief Prefixes an error's message with its location inside a container.
 * @param err     The error from the nested conversion.
 * @param segment Location text such as "[3]" or "[\"name\"]".
 */
inline LuaError withPath(LuaError err, const std::string& segment) {
	err.message = segment + ": " + err.message;
	return err;
}

/**
 * @brief Converts a Lua table to a string-keyed std::map or std::unordered_map.
 *
 * Every key must be a string. Keys are copied; values are converted with
 * LuaConverter<mapped_type>.
 */
template <typename Map>
LuaResult<Map> mapFromLua(lua_State* L, int index) {
	using Value = typename Map::mapped_type;

	if (lua_type(L, index) != LUA_TTABLE)
		return LuaError{ "table expected", index };

	NestingGuard guard;
	if (!guard.ok())
		return LuaError{ "table nesting too deep", index };

	const int absIndex = lua_absindex(L, index);
	const int base = lua_gettop(L);

	Map out;
	size_t count = 0;

	lua_pushnil(L);
	while (true) {
		if (!lua_checkstack(L, 3)) {
			lua_settop(L, base);
			return LuaError{ "Lua stack overflow", index };
		}

		if (lua_next(L, absIndex) == 0)
			break;

		if (++count > kMaxContainerElements) {
			lua_settop(L, base);
			return LuaError{ "table too large", index };
		}

		if (lua_type(L, -2) != LUA_TSTRING) {
			lua_settop(L, base);
			return LuaError{ "string keys required", index };
		}

		size_t keyLength = 0;
		const char* keyData = lua_tolstring(L, -2, &keyLength);
		std::string key(keyData, keyLength);

		LuaResult<Value> value = LuaConverter<Value>::fromLua(L, -1);
		if (!value.ok()) {
			LuaError err = withPath(value.error(), "[\"" + key + "\"]");
			lua_settop(L, base);
			return err;
		}

		out.emplace(std::move(key), std::move(value.value()));
		lua_pop(L, 1);   // keep the key for lua_next
	}

	lua_settop(L, base);
	return out;
}

/**
 * @brief Pushes a string-keyed map as a new Lua table.
 */
template <typename Map>
void mapToLua(lua_State* L, const Map& map) {
	using Value = typename Map::mapped_type;

	luaL_checkstack(L, 2, "LuaConverter: map push");
	lua_createtable(L, 0, static_cast<int>(map.size()));

	for (const auto& [key, value] : map) {
		lua_pushlstring(L, key.data(), key.size());
		LuaConverter<Value>::push(L, value);
		lua_rawset(L, -3);
	}
}

} // namespace Detail

/**
 * @brief Converts a Lua number to int.
 *
 * Accepts integer values and integral floats (so `n / 2` works). Rejects
 * strings, booleans, non-integral values, NaN, infinities, and out-of-range
 * values.
 */
template <>
struct LuaConverter<int> {
	static LuaResult<int> fromLua(lua_State* L, int index) {
		if (lua_type(L, index) != LUA_TNUMBER)
			return LuaError{ "integer expected", index };

		if (lua_isinteger(L, index)) {
			const lua_Integer v = lua_tointeger(L, index);
			if (v < std::numeric_limits<int>::min() || v > std::numeric_limits<int>::max())
				return LuaError{ "integer out of range", index };

			return static_cast<int>(v);
		}

		const double d = lua_tonumber(L, index);
		if (std::isnan(d) || d != std::floor(d))
			return LuaError{ "integer expected", index };

		if (d < static_cast<double>(std::numeric_limits<int>::min()) ||
			d > static_cast<double>(std::numeric_limits<int>::max()))
			return LuaError{ "integer out of range", index };

		return static_cast<int>(d);
	}

	static void push(lua_State* L, int value) {
		lua_pushinteger(L, value);
	}
};

/// Converts a Lua number to double.
template <>
struct LuaConverter<double> {
	static LuaResult<double> fromLua(lua_State* L, int index) {
		if (lua_type(L, index) != LUA_TNUMBER)
			return LuaError{ "number expected", index };

		return lua_tonumber(L, index);
	}

	static void push(lua_State* L, double value) {
		lua_pushnumber(L, value);
	}
};

/// Converts a Lua number to float via double.
template <>
struct LuaConverter<float> {
	static LuaResult<float> fromLua(lua_State* L, int index) {
		auto d = LuaConverter<double>::fromLua(L, index);
		if (!d.ok())
			return d.error();

		return static_cast<float>(d.value());
	}

	static void push(lua_State* L, float value) {
		lua_pushnumber(L, static_cast<lua_Number>(value));
	}
};

/// Converts a Lua boolean. Other types, including nil and numbers, are errors.
template <>
struct LuaConverter<bool> {
	static LuaResult<bool> fromLua(lua_State* L, int index) {
		if (!lua_isboolean(L, index))
			return LuaError{ "boolean expected", index };

		return lua_toboolean(L, index) != 0;
	}

	static void push(lua_State* L, bool value) {
		lua_pushboolean(L, value ? 1 : 0);
	}
};

/// Converts a Lua string. Copies the bytes, so the result outlives the stack value.
template <>
struct LuaConverter<std::string> {
	static LuaResult<std::string> fromLua(lua_State* L, int index) {
		if (lua_type(L, index) != LUA_TSTRING)
			return LuaError{ "string expected", index };

		size_t length = 0;
		const char* data = lua_tolstring(L, index, &length);
		return std::string(data, length);
	}

	static void push(lua_State* L, const std::string& value) {
		lua_pushlstring(L, value.data(), value.size());
	}
};

/**
 * @brief Converts a Lua table used as a sequence to std::vector<T>.
 *
 * Reads indices 1 through #t with raw access. A nil at any index is an error.
 * Other keys are ignored. Elements are converted with LuaConverter<T>, and
 * errors carry the failing index, for example `[3]: integer expected`.
 */
template <typename T>
struct LuaConverter<std::vector<T>> {
	static LuaResult<std::vector<T>> fromLua(lua_State* L, int index) {
		if (lua_type(L, index) != LUA_TTABLE)
			return LuaError{ "table expected", index };

		Detail::NestingGuard guard;
		if (!guard.ok())
			return LuaError{ "table nesting too deep", index };

		const int absIndex = lua_absindex(L, index);
		const lua_Unsigned length = lua_rawlen(L, absIndex);
		if (length > Detail::kMaxContainerElements)
			return LuaError{ "sequence too long", index };

		std::vector<T> out;
		out.reserve(static_cast<size_t>(length));

		for (lua_Integer i = 1; i <= static_cast<lua_Integer>(length); ++i) {
			if (!lua_checkstack(L, 1))
				return LuaError{ "Lua stack overflow", index };

			lua_rawgeti(L, absIndex, i);
			LuaResult<T> element = LuaConverter<T>::fromLua(L, -1);
			lua_pop(L, 1);

			if (!element.ok())
				return Detail::withPath(element.error(), "[" + std::to_string(i) + "]");

			out.push_back(std::move(element.value()));
		}

		return out;
	}

	/**
	 * @brief Pushes the vector as a new table with elements at indices 1..n.
	 *
	 * May raise a Lua error if the stack cannot grow.
	 */
	static void push(lua_State* L, const std::vector<T>& values) {
		luaL_checkstack(L, 2, "LuaConverter: vector push");
		lua_createtable(L, static_cast<int>(values.size()), 0);

		lua_Integer i = 1;
		for (const T& element : values) {
			LuaConverter<T>::push(L, element);
			lua_rawseti(L, -2, i++);
		}
	}
};

/**
 * @brief Converts a string-keyed Lua table to std::map<std::string, V>.
 *
 * Every key must be a string. Non-string keys are an error, so data is never
 * dropped silently. Values are converted with LuaConverter<V>.
 */
template <typename V>
struct LuaConverter<std::map<std::string, V>> {
	static LuaResult<std::map<std::string, V>> fromLua(lua_State* L, int index) {
		return Detail::mapFromLua<std::map<std::string, V>>(L, index);
	}

	static void push(lua_State* L, const std::map<std::string, V>& map) {
		Detail::mapToLua(L, map);
	}
};

/**
 * @brief Converts a string-keyed Lua table to std::unordered_map<std::string, V>.
 *
 * Same rules as the std::map specialization.
 */
template <typename V>
struct LuaConverter<std::unordered_map<std::string, V>> {
	static LuaResult<std::unordered_map<std::string, V>> fromLua(lua_State* L, int index) {
		return Detail::mapFromLua<std::unordered_map<std::string, V>>(L, index);
	}

	static void push(lua_State* L, const std::unordered_map<std::string, V>& map) {
		Detail::mapToLua(L, map);
	}
};

} // namespace Blackthorn::Scripting