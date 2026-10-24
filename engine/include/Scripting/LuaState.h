#pragma once

#include <concepts>
#include <cstddef>
#include <exception>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include <lua.h>

#include "Scripting/LuaConverter.h"
#include "Scripting/LuaResult.h"

namespace Blackthorn::Scripting {

/**
 * @brief A type that a LuaConverter can read from the Lua stack.
 */
template <typename T>
concept LuaReadable = requires(lua_State* L, int index) {
	{ LuaConverter<T>::fromLua(L, index) } -> std::same_as<LuaResult<T>>;
};

/**
 * @brief A type that a LuaConverter can push onto the Lua stack.
 */
template <typename T>
concept LuaWritable = requires(lua_State* L, const T& value) {
	LuaConverter<T>::push(L, value);
};

namespace Detail {

/**
 * @brief Decomposes a function pointer type into its return and parameter types.
 */
template <typename>
struct FunctionTraits;

template <typename R, typename... A>
struct FunctionTraits<R(*)(A...)> {
	using Return = R;
	using Params = std::tuple<std::remove_cvref_t<A>...>;
	using RawArgs = std::tuple<A...>;
};

template <typename R, typename... A>
struct FunctionTraits<R(*)(A...) noexcept> : FunctionTraits<R(*)(A...)> {};

/**
 * @brief True if every parameter type has a readable converter.
 */
template <typename Params, size_t... I>
constexpr bool allReadable(std::index_sequence<I...>) {
	return (LuaReadable<std::tuple_element_t<I, Params>> && ...);
}

/**
 * @brief Converts Lua stack values 1..N into the parameter tuple.
 *
 * Stops at the first failure. Each failure carries the 1-based Lua index of
 * the argument, which becomes the "#k" in the error message.
 *
 * Everything here may hold C++ objects, so this must run inside a scope that
 * ends before any Lua error is raised.
 */
template <typename Params, size_t... I>
std::optional<LuaError> convertArgs(lua_State* L, Params& params, std::index_sequence<I...>) {
	std::optional<LuaError> error;

	([&] {
		if (error)
			return;
		using T = std::tuple_element_t<I, Params>;
		LuaResult<T> converted = LuaConverter<T>::fromLua(L, static_cast<int>(I) + 1);
		if (!converted.ok()) {
			error = converted.error();
			return;
		}
		std::get<I>(params) = std::move(converted.value());
	}(), ...);

	return error;
}

/**
 * @brief Performs the whole call: arity check, argument conversion, the call
 *        itself, and result push.
 *
 * C++ exceptions from the engine function are caught here and turned into
 * Lua errors. Only std::exception is caught. Lua's own errors in C++ mode are
 * thrown as internal types, and catching those would swallow script errors.
 *
 * @return The number of results pushed, or an error for the caller to raise.
 */
template <auto Fn>
LuaResult<int> invokeRegistered(lua_State* L) {
	using Traits = FunctionTraits<decltype(Fn)>;
	using R = typename Traits::Return;
	using Params = typename Traits::Params;
	constexpr size_t N = std::tuple_size_v<Params>;

	static_assert(allReadable<Params>(std::make_index_sequence<N>{}),
		"registerFunction: a parameter type has no LuaConverter specialization");
	static_assert(!(std::is_rvalue_reference_v<typename Traits::RawArgs>),
		"registerFunction: rvalue reference parameters are not supported");

	const int given = lua_gettop(L);
	if (given != static_cast<int>(N)) {
		return LuaError{
			"expected " + std::to_string(N) + " arguments, got " + std::to_string(given),
			0
		};
	}

	Params params{};
	if (std::optional<LuaError> error = convertArgs(L, params, std::make_index_sequence<N>{})) {
		return *error;
	}

	try {
		if constexpr (std::is_void_v<R>) {
			std::apply(Fn, params);
			return 0;
		} else {
			static_assert(LuaWritable<R>,
				"registerFunction: return type has no LuaConverter specialization");
			R result = std::apply(Fn, params);
			LuaConverter<R>::push(L, result);
			return 1;
		}
	} catch (const std::exception& e) {
		return LuaError{ e.what(), 0 };
	}
}

/**
 * @brief The Lua C function installed for each registered function.
 *
 * Upvalue 1 holds the registered name, used in error messages. Raising the
 * error happens only here, after invokeRegistered has returned and destroyed
 * every C++ object it created.
 */
template <auto Fn>
int dispatch(lua_State* L) {
	LuaResult<int> result = invokeRegistered<Fn>(L);
	if (result.ok())
		return result.value();

	const LuaError& error = result.error();
	const char* name = lua_tostring(L, lua_upvalueindex(1));

	if (error.argIndex > 0) {
		lua_pushfstring(L, "bad argument #%d to '%s' (%s)",
			error.argIndex, name, error.message.c_str());
	} else {
		lua_pushfstring(L, "'%s': %s", name, error.message.c_str());
	}
	return lua_error(L);
}

} // namespace Detail

/**
 * @brief Registers a free function as a Lua global-style field.
 *
 * The table must be at the top of the stack. The function is stored in the
 * table under @p name, and the table is left on the stack.
 *
 * Arguments are checked strictly: a call with the wrong number of arguments
 * is an error. Each argument is converted with LuaConverter<T>. Results are
 * pushed with LuaConverter<R>::push. C++ exceptions from @p Fn become Lua
 * errors that carry the exception's message.
 *
 * Supported: free functions, including noexcept ones, with parameters and
 * return types that have LuaConverter specializations.
 * Not yet supported: member functions, lambdas with captures, and multiple
 * return values.
 *
 * @tparam Fn Function pointer, supplied as a template argument.
 * @param L    Lua state with the target table on top of the stack.
 * @param name Name the script will call the function by.
 */
template <auto Fn>
void registerFunction(lua_State* L, const char* name) {
	static_assert(std::is_pointer_v<decltype(Fn)> &&
		std::is_function_v<std::remove_pointer_t<decltype(Fn)>>,
		"registerFunction requires a free function pointer");

	lua_pushstring(L, name);                              // upvalue: name for errors
	lua_pushcclosure(L, &Detail::dispatch<Fn>, 1);
	lua_setfield(L, -2, name);                            // table[name] = closure
}

} // namespace Blackthorn::Scripting