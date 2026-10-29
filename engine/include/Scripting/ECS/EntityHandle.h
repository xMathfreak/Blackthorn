#pragma once

#include "ECS/Entity.h"
#include "Scripting/LuaConverter.h"

namespace Blackthorn::Scripting {

namespace ECS {

/// Metatable name for entity handles.
inline constexpr const char* kEntityMetatable = "Blackthorn.Entity";

/// Metatable name for component proxies.
inline constexpr const char* kComponentMetatable = "Blackthorn.Component";

/**
 * @brief Script-visible entity reference.
 *
 * Wraps ECS::Entity so it has its own converter and cannot be confused with
 * an ordinary integer. Validity is decided by EntityPool::isValid, which
 * checks the generation packed into the value.
 */
struct EntityHandle {
	Blackthorn::ECS::Entity value = Blackthorn::ECS::INVALID_ENTITY;

	friend bool operator==(EntityHandle, EntityHandle) = default;
};

/**
 * @brief Full userdata block for an entity handle.
 */
struct EntityUserdata {
	Blackthorn::ECS::Entity value;
};

} // namespace Blackthorn::Scripting::ECS

/**
 * @brief Converts between EntityHandle and Lua userdata with kEntityMetatable.
 *
 * Plain integers are rejected. A handle must come from the engine or from
 * a previous script call, never from arithmetic.
 */
template <>
struct LuaConverter<ECS::EntityHandle> {
	static LuaResult<ECS::EntityHandle> fromLua(lua_State* L, int index) {
		const auto* data = static_cast<const ECS::EntityUserdata*>(
			luaL_testudata(L, index, ECS::kEntityMetatable)
		);

		if (!data)
			return LuaError{"entity handle expected", index};

		return ECS::EntityHandle{data->value};
	}

	static void push(lua_State* L, ECS::EntityHandle handle) {
		auto* data = static_cast<ECS::EntityUserdata*>(
			lua_newuserdatauv(L, sizeof(ECS::EntityUserdata), 0)
		);
		data->value = handle.value;
		luaL_setmetatable(L, ECS::kEntityMetatable);
	}
};

} // namespace Blackthorn::Scripting