#pragma once

#include <cstddef>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include <lua.h>

#include "ECS/EntityPool.h"
#include "Scripting/ECS/EntityHandle.h"
#include "Scripting/LuaConverter.h"
#include "Scripting/LuaResult.h"
#include "Scripting/LuaState.h"
#include "Types/StringHash.h"

namespace Blackthorn::Scripting::ECS {

class ScriptWorld;

/// Names used by entity methods.
inline const std::set<std::string, std::less<>> reservedComponentNames = {
	"addComponent",
	"removeComponent",
	"hasComponent",
	"addTag",
	"removeTag",
	"hasTag"
};

/**
 * @brief Type-erased access to one property. Built by ScriptBindings from typed lambdas.
 *
 * Getter pushes the value at the top of the stack. Setter reads the new value
 * from stack index @c valueIndex. Both report failure as a LuaError and never throw.
 */
struct ScriptProperty {
	using Getter = std::function<LuaResult<int>(ScriptWorld&, EntityHandle, lua_State*)>;
	using Setter = std::function<LuaResult<void>(ScriptWorld&, EntityHandle, lua_State*, int valueIndex)>;

	Getter getter;
	Setter setter;
};


/// Everything registered for one component.
struct ComponentRecord {
	std::string scriptName;

	std::function<bool(Blackthorn::ECS::EntityPool&, Blackthorn::ECS::Entity)> has;
	std::function<void(Blackthorn::ECS::EntityPool&, Blackthorn::ECS::Entity)> add;
	std::function<void(Blackthorn::ECS::EntityPool&, Blackthorn::ECS::Entity)> remove;

	std::unordered_map<std::string, ScriptProperty, StringHash, std::equal_to<>> properties;
};

namespace Detail {

/// Extracts return and argument types from a non-generic lambda or function object.
template <typename F>
struct CallableTraits : CallableTraits<decltype(&F::operator())> {};

template <typename C, typename R, typename... A>
struct CallableTraits<R (C::*)(A...) const> {
	using Return = R;
	using Args = std::tuple<A...>;
};

} // namespace Detail

/**
 * @brief Registry of script-visible component properties.
 *
 * Components are declared once under a script name. Properties are then
 * registered under that component. Scripts address a property by its
 * qualified name, for example "Transform.position".
 *
 * Setup only: register everything before any script runs, then treat the
 * bindings as read-only. Not thread-safe.
 */
class ComponentBindings {
public:
	/**
	 * @brief Makes a default constructible component addable from scripts.
	 */
	template <typename Component>
	void declareComponent(std::string_view scriptName) {
		static_assert(std::is_default_constructible_v<Component>,
			"declareComponent: component is not default constructible"
		);

		declareComponent<Component>(scriptName, [] { return Component{}; });
	}

	template <typename Component, typename Factory>
	void declareComponent(std::string_view scriptName, Factory factory) {
		static_assert(std::is_invocable_r_v<Component, Factory&>,
			"declareComponent: factory must return the component type"
		);

		const size_t id = Blackthorn::ECS::Detail::componentID<Component>();

		if (reservedComponentNames.contains(scriptName))
			throw std::logic_error("component name '" + std::string(scriptName) + "' is reserved for entity methods");

		if (idByName.find(scriptName) != idByName.end())
			throw std::logic_error("script component name '" + std::string(scriptName) + "' already declared");

		if (components.find(id) != components.end())
			throw std::logic_error("component '" + std::string(scriptName) + "' is already declared for scripts");

		ComponentRecord record;
		record.scriptName = std::string(scriptName);

		record.has = [](Blackthorn::ECS::EntityPool& pool, Blackthorn::ECS::Entity entity) {
			return pool.hasComponent<Component>(entity);
		};

		record.remove = [](Blackthorn::ECS::EntityPool& pool, Blackthorn::ECS::Entity entity) {
			pool.removeComponent<Component>(entity);
		};

		record.add = [factory = std::move(factory)](Blackthorn::ECS::EntityPool& pool, Blackthorn::ECS::Entity entity) {
			pool.addComponent<Component>(entity, factory());
		};

		idByName.emplace(record.scriptName, id);
		components.emplace(id, std::move(record));
	}

	/**
	 * @brief Registers a read property. @p fn is `(ScriptWorld&, EntityHandle) -> T`.
	 * @throws std::logic_error if the component was not declared.
	 */
	template <typename Component, typename Fn>
	void registerGetter(std::string_view property, Fn fn) {
		using Traits = Detail::CallableTraits<Fn>;
		using Args = typename Traits::Args;
		using Value = std::remove_cvref_t<typename Traits::Return>;

		static_assert(std::is_same_v<std::tuple_element_t<0, Args>, ScriptWorld&>,
			"getter must take ScriptWorld& as its first parameter");

		static_assert(std::is_same_v<std::tuple_element_t<1, Args>, EntityHandle>,
			"getter must take EntityHandle as its second parameter");

		static_assert(LuaWritable<Value>, "getter return type has no LuaConverter");

		ScriptProperty& prop = propertyFor<Component>(property);
		prop.getter = [fn = std::move(fn)](ScriptWorld& world, EntityHandle handle,
			lua_State* L) -> LuaResult<int> {
			// Catch here, not in the caller: a C++ exception must not cross
			// Lua's C frames, where its message would be lost.
			try {
				LuaConverter<Value>::push(L, fn(world, handle));
			} catch (const std::exception& e) {
				return LuaError{ e.what(), 0 };
			}
			return 1;
		};
	}

	/**
	 * @brief Registers a write property. @p fn is `(ScriptWorld&, EntityHandle, T) -> void`.
	 * @throws std::logic_error if the component was not declared.
	 */
	template <typename Component, typename Fn>
	void registerSetter(std::string_view property, Fn fn) {
		using Traits = Detail::CallableTraits<Fn>;
		using Args = typename Traits::Args;
		using Value = std::remove_cvref_t<std::tuple_element_t<2, Args>>;

		static_assert(std::is_same_v<std::tuple_element_t<0, Args>, ScriptWorld&>,
			"setter must take ScriptWorld& as its first parameter");

		static_assert(std::is_same_v<std::tuple_element_t<1, Args>, EntityHandle>,
			"setter must take EntityHandle as its second parameter");

		static_assert(LuaReadable<Value>, "setter value type has no LuaConverter");

		ScriptProperty& prop = propertyFor<Component>(property);
		prop.setter = [fn = std::move(fn)](ScriptWorld& world, EntityHandle handle,
			lua_State* L, int valueIndex) -> LuaResult<void> {
			LuaResult<Value> value = LuaConverter<Value>::fromLua(L, valueIndex);
			if (!value.ok())
				return value.error();

			try {
				fn(world, handle, std::move(value.value()));
			} catch (const std::exception& e) {
				return LuaError{ e.what(), 0 };
			}
			return LuaResult<void>{};
		};
	}

	/**
	 * @brief Finds a property by script names, for example ("Transform", "position").
	 * @return The property, or nullptr if either name is unknown.
	 */
	const ScriptProperty* findProperty(std::string_view component, std::string_view property) const {
		const auto idIt = idByName.find(component);
		if (idIt == idByName.end())
			return nullptr;

		const auto recIt = components.find(idIt->second);
		if (recIt == components.end())
			return nullptr;

		const auto propIt = recIt->second.properties.find(property);
		return propIt == recIt->second.properties.end() ? nullptr : &propIt->second;
	}

	/// @return The record for a script component name or nullptr if undeclared.
	const ComponentRecord* findComponent(std::string_view name) const {
		const auto idIt = idByName.find(name);
		if (idIt == idByName.end())
			return nullptr;

		return &components.at(idIt->second);
	}

private:
	template <typename Component>
	ScriptProperty& propertyFor(std::string_view property) {
		const size_t id = Blackthorn::ECS::Detail::componentID<Component>();

		const auto recIt = components.find(id);
		if (recIt == components.end())
			throw std::logic_error("component must be declared before its properties are registered");

		return recIt->second.properties.try_emplace(std::string(property)).first->second;
	}

	/// Script name -> component ID. Transparent lookup so string_view queries don't allocate.
	std::unordered_map<std::string, size_t, StringHash, std::equal_to<>> idByName;

	/// Component ID -> its script name and properties.
	std::unordered_map<size_t, ComponentRecord> components;
};

} // namespace Blackthorn::Scripting::ECS