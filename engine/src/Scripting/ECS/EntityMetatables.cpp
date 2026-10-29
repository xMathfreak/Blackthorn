#include "Scripting/ECS/EntityMetatable.h"

#include <exception>
#include <string>
#include <string_view>

#include <lauxlib.h>

#include "ECS/Detail.h"
#include "Scripting/LuaResult.h"
#include "Scripting/ECS/EntityApi.h"
#include "Scripting/ECS/EntityHandle.h"
#include "Scripting/ECS/ComponentBindings.h"

namespace Blackthorn::Scripting::ECS {

namespace {

/// Userdata for components e.g `e.Transform`
struct ComponentUserdata {
	Blackthorn::ECS::Entity value;
	const ComponentRecord* record;
};

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

const EntityUserdata* entityArg(lua_State* L, int index) {
	return static_cast<const EntityUserdata*>(luaL_testudata(L, index, kEntityMetatable));
}

const ComponentUserdata* componentArg(lua_State* L, int index) {
	return static_cast<const ComponentUserdata*>(luaL_testudata(L, index, kComponentMetatable));
}

/// A string argument. The view points into the Lua string, which stays valid while it is on the stack.
LuaResult<std::string_view> stringArg(lua_State* L, int index) {
	if (lua_type(L, index) != LUA_TSTRING)
		return LuaError{ "name must be a string", index };

	size_t length = 0;
	const char* text = lua_tolstring(L, index, &length);
	return std::string_view(text, length);
}

/// Index of the first real argument: 2 for a colon call (self at 1), 1 for a dot call.
int argsOffset(lua_State* L) {
	return luaL_testudata(L, 1, kEntityMetatable) ? 2 : 1;
}

/// Applies a table of initial values through the component's property setters.
/// Keys must be property names, and each value goes to the same setter that
/// assignment uses, so validation is identical.
LuaResult<void> applyInitialValues(
	lua_State* L,
	ScriptWorld& world,
	EntityHandle handle,
	const ComponentRecord& record,
	int tableIndex
) {
	if (lua_type(L, tableIndex) != LUA_TTABLE)
		return LuaError{ "initial values must be a table", tableIndex };

	const int table = lua_absindex(L, tableIndex);
	const int base = lua_gettop(L);

	lua_pushnil(L);
	while (lua_next(L, table) != 0) {
		if (lua_type(L, -2) != LUA_TSTRING) {
			lua_settop(L, base);
			return LuaError{ "property names must be strings", 0 };
		}

		size_t length = 0;
		const char* text = lua_tolstring(L, -2, &length);
		const std::string_view name(text, length);

		const auto it = record.properties.find(name);
		if (it == record.properties.end()) {
			lua_settop(L, base);
			return LuaError{ "unknown property '" + std::string(name)
				+ "' on " + record.scriptName, 0 };
		}

		if (!it->second.setter) {
			lua_settop(L, base);
			return LuaError{ "property '" + std::string(name) + "' is read-only", 0 };
		}

		// The value is on top of the stack, so its absolute index is lua_gettop.
		LuaResult<void> set = it->second.setter(world, handle, L, lua_gettop(L));
		if (!set.ok()) {
			LuaError error = set.error();
			lua_settop(L, base);
			return error;
		}

		lua_pop(L, 1);   // keep the key for lua_next
	}

	lua_settop(L, base);
	return LuaResult<void>{};
}

/// Adds a component and optionally initializes it. If initialization fails,
/// the component is removed, so a failed add leaves nothing behind.
LuaResult<int> addComponentWithValues(lua_State* L, EntityHandle handle, int nameIndex, int initIndex) {
	LuaResult<std::string_view> name = stringArg(L, nameIndex);
	if (!name.ok())
		return name.error();

	ScriptWorld& world = Detail::requireWorld();
	const ComponentRecord& record = world.attachComponent(handle, name.value());

	if (!lua_isnoneornil(L, initIndex)) {
		LuaResult<void> init = applyInitialValues(L, world, handle, record, initIndex);
		if (!init.ok()) {
			LuaError error = init.error();
			world.removeComponent(handle, record.scriptName);
			return error;
		}
	}

	return 0;
}

LuaResult<int> removeComponentByName(lua_State* L, EntityHandle handle, int nameIndex) {
	LuaResult<std::string_view> name = stringArg(L, nameIndex);
	if (!name.ok())
		return name.error();

	Detail::requireWorld().removeComponent(handle, name.value());
	return 0;
}

LuaResult<int> hasComponentByName(lua_State* L, EntityHandle handle, int nameIndex) {
	LuaResult<std::string_view> name = stringArg(L, nameIndex);
	if (!name.ok())
		return name.error();

	const bool has = Detail::requireWorld().hasComponent(handle, name.value());
	lua_pushboolean(L, has ? 1 : 0);
	return 1;
}

/// The entity a bound method was read from, stored as the closure's upvalue.
LuaResult<EntityHandle> boundEntity(lua_State* L) {
	const auto* self = static_cast<const EntityUserdata*>(
		luaL_testudata(L, lua_upvalueindex(1), kEntityMetatable));
	if (!self)
		return LuaError{ "entity expected", 0 };
	return EntityHandle{ self->value };
}

int methodAddComponent(lua_State* L) {
	return runMetamethod(L, [&]() -> LuaResult<int> {
		LuaResult<EntityHandle> self = boundEntity(L);
		if (!self.ok())
			return self.error();

		const int first = argsOffset(L);
		return addComponentWithValues(L, self.value(), first, first + 1);
	});
}

int methodRemoveComponent(lua_State* L) {
	return runMetamethod(L, [&]() -> LuaResult<int> {
		LuaResult<EntityHandle> self = boundEntity(L);
		if (!self.ok())
			return self.error();

		return removeComponentByName(L, self.value(), argsOffset(L));
	});
}

int methodHasComponent(lua_State* L) {
	return runMetamethod(L, [&]() -> LuaResult<int> {
		LuaResult<EntityHandle> self = boundEntity(L);
		if (!self.ok())
			return self.error();

		return hasComponentByName(L, self.value(), argsOffset(L));
	});
}

/// Pushes a method bound to the entity at index 1, and returns 1.
int pushMethod(lua_State* L, lua_CFunction fn) {
	lua_pushvalue(L, 1);
	lua_pushcclosure(L, fn, 1);
	return 1;
}

/// Shared body for tag methods: resolves the entity and the tag name, then runs @p op.
template <typename Op>
int tagMethod(lua_State* L, Op op) {
	return runMetamethod(L, [&]() -> LuaResult<int> {
		LuaResult<EntityHandle> self = boundEntity(L);
		if (!self.ok())
			return self.error();

		LuaResult<std::string_view> tag = stringArg(L, argsOffset(L));
		if (!tag.ok())
			return tag.error();

		return op(Detail::requireWorld(), self.value(), tag.value(), L);
	});
}

int methodAddTag(lua_State* L) {
	return tagMethod(L, [](ScriptWorld& w, EntityHandle h, std::string_view t, lua_State*) -> LuaResult<int> {
		w.addTag(h, t);
		return 0;
	});
}

int methodRemoveTag(lua_State* L) {
	return tagMethod(L, [](ScriptWorld& w, EntityHandle h, std::string_view t, lua_State*) -> LuaResult<int> {
		w.removeTag(h, t);
		return 0;
	});
}

int methodHasTag(lua_State* L) {
	return tagMethod(L, [](ScriptWorld& w, EntityHandle h, std::string_view t, lua_State* s) -> LuaResult<int> {
		lua_pushboolean(s, w.hasTag(h, t) ? 1 : 0);
		return 1;
	});
}

LuaResult<int> indexEntity(lua_State* L, ComponentBindings& bindings) {
	if (!entityArg(L, 1))
		return LuaError{ "entity handle expected", 1 };

	LuaResult<std::string_view> key = stringArg(L, 2);
	if (!key.ok())
		return key.error();

	// Methods are checked first. Declaration rejects component names that match them.
	if (key.value() == "addComponent")
		return pushMethod(L, methodAddComponent);

	if (key.value() == "removeComponent")
		return pushMethod(L, methodRemoveComponent);

	if (key.value() == "hasComponent")
		return pushMethod(L, methodHasComponent);

	if (key.value() == "addTag")
		return pushMethod(L, methodAddTag);

	if (key.value() == "removeTag")
		return pushMethod(L, methodRemoveTag);

	if (key.value() == "hasTag")
		return pushMethod(L, methodHasTag);

	const ComponentRecord* record = bindings.findComponent(key.value());
	if (!record)
		return LuaError{ "unknown component '" + std::string(key.value()) + "'", 0 };

	// A stale handle is reported when a property is read or written, not here,
	// so both paths produce the same error.
	const EntityUserdata* self = entityArg(L, 1);
	auto* proxy = static_cast<ComponentUserdata*>(
		lua_newuserdatauv(L, sizeof(ComponentUserdata), 0)
	);

	proxy->value = self->value;
	proxy->record = record;
	luaL_setmetatable(L, kComponentMetatable);
	return 1;
}

int entityIndexMeta(lua_State* L) {
	auto* bindings = static_cast<ComponentBindings*>(lua_touserdata(L, lua_upvalueindex(1)));
	return runMetamethod(L, [&]() { return indexEntity(L, *bindings); });
}

int entityNewIndexMeta(lua_State* L) {
	return runMetamethod(L, [&]() -> LuaResult<int> {
		return LuaError{
			"components cannot be assigned directly; assign a property, for example e.Transform.position = ...",
			0
		};
	});
}

int entityEqMeta(lua_State* L) {
	const EntityUserdata* a = entityArg(L, 1);
	const EntityUserdata* b = entityArg(L, 2);
	lua_pushboolean(L, a && b && a->value == b->value);
	return 1;
}

int entityToStringMeta(lua_State* L) {
	const EntityUserdata* self = entityArg(L, 1);
	if (!self) {
		lua_pushliteral(L, "Entity(invalid)");
		return 1;
	}

	lua_pushfstring(L, "Entity(%d:%d)",
		static_cast<int>(Blackthorn::ECS::Detail::entityIndex(self->value)),
		static_cast<int>(Blackthorn::ECS::Detail::entityGeneration(self->value)));
	return 1;
}

LuaResult<int> indexComponent(lua_State* L) {
	const ComponentUserdata* self = componentArg(L, 1);
	if (!self)
		return LuaError{ "component expected", 1 };

	LuaResult<std::string_view> key = stringArg(L, 2);
	if (!key.ok())
		return key.error();

	const auto it = self->record->properties.find(key.value());
	if (it == self->record->properties.end())
		return LuaError{ "unknown property '" + std::string(key.value())
			+ "' on " + self->record->scriptName, 0 };

	if (!it->second.getter)
		return LuaError{ "property '" + std::string(key.value()) + "' cannot be read", 0 };

	// The getter pushes its result, and failures come back as a LuaError.
	return it->second.getter(Detail::requireWorld(), EntityHandle{ self->value }, L);
}

LuaResult<int> newIndexComponent(lua_State* L) {
	const ComponentUserdata* self = componentArg(L, 1);
	if (!self)
		return LuaError{ "component expected", 1 };

	LuaResult<std::string_view> key = stringArg(L, 2);
	if (!key.ok())
		return key.error();

	const auto it = self->record->properties.find(key.value());
	if (it == self->record->properties.end())
		return LuaError{ "unknown property '" + std::string(key.value())
			+ "' on " + self->record->scriptName, 0 };

	if (!it->second.setter)
		return LuaError{ "property '" + std::string(key.value()) + "' is read-only", 0 };

	// __newindex(proxy, key, value): the new value is at index 3.
	LuaResult<void> set = it->second.setter(Detail::requireWorld(),
		EntityHandle{ self->value }, L, 3);

	if (!set.ok())
		return set.error();

	return 0;
}

int componentIndexMeta(lua_State* L) {
	return runMetamethod(L, [&]() { return indexComponent(L); });
}

int componentNewIndexMeta(lua_State* L) {
	return runMetamethod(L, [&]() { return newIndexComponent(L); });
}

} // namespace

void installEntityMetatables(lua_State* L, ComponentBindings& bindings) {
	luaL_newmetatable(L, kEntityMetatable);

	lua_pushlightuserdata(L, &bindings);
	lua_pushcclosure(L, entityIndexMeta, 1);
	lua_setfield(L, -2, "__index");

	lua_pushcfunction(L, entityNewIndexMeta);
	lua_setfield(L, -2, "__newindex");

	lua_pushcfunction(L, entityEqMeta);
	lua_setfield(L, -2, "__eq");

	lua_pushcfunction(L, entityToStringMeta);
	lua_setfield(L, -2, "__tostring");

	// Scripts can't replace the metatable of a handle.
	lua_pushboolean(L, 0);
	lua_setfield(L, -2, "__metatable");

	lua_pop(L, 1);

	luaL_newmetatable(L, kComponentMetatable);

	lua_pushcfunction(L, componentIndexMeta);
	lua_setfield(L, -2, "__index");

	lua_pushcfunction(L, componentNewIndexMeta);
	lua_setfield(L, -2, "__newindex");

	lua_pushboolean(L, 0);
	lua_setfield(L, -2, "__metatable");

	lua_pop(L, 1);
}

} // namespace Blackthorn::Scripting