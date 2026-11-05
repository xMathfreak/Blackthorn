#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>

#include <lua.h>
#include <lauxlib.h>

#include "Core/Export.h"
#include "Debug/Logger.h"
#include "Scripting/LuaConverter.h"
#include "Scripting/LuaResult.h"

namespace Blackthorn::Scripting {

/**
 * @brief Resource limits applied to a LuaScript.
 */
struct LuaScriptConfig {
	size_t memoryLimitBytes = 8 * 1024 * 1024;         ///< Hard cap on Lua heap usage after setup.
	std::chrono::milliseconds timeSlice{ 16 };   ///< Wall-clock budget per outermost call.
	unsigned int hookInterval = 10000;                 ///< VM instructions between deadline checks.

	///Receives each line printed by scripts. Null writes to stdout.
	std::function<void(std::string_view)> printSink = [](std::string_view line) {
		BT_TAGGED(Debug::LogTag::Script, Debug::LogLevel::Info, "{}", line);
	};

	/// Longest line passed to the sink. Longer output is truncated.
	size_t maxPrintBytes = 4096;
};

/**
 * @brief Owns one sandboxed Lua state and the environment its scripts run in.
 *
 * Scripts see only the environment table built by the sandbox and whatever
 * registerApi() adds. They cannot reach io, os, debug, package, load, or
 * bytecode loading.
 *
 * Thread-affine: create, use, and destroy on one thread.
 * Non-movable: the allocator and hook hold back-pointers to this object.
 */
class BLACKTHORN_API LuaScript {
public:
	/**
	 * @brief Creates a sandboxed state.
	 * @throws std::runtime_error if the Lua state cannot be created.
	 */
	explicit LuaScript(LuaScriptConfig config = {});

	/**
	 * @brief Closes the state. Finalizers run now, so engine objects they touch
	 *        must still be alive. Destroy LuaScript before the systems it uses.
	 */
	~LuaScript();

	LuaScript(const LuaScript&) = delete;
	LuaScript& operator=(const LuaScript&) = delete;
	LuaScript(LuaScript&&) = delete;
	LuaScript& operator=(LuaScript&&) = delete;

	/**
	 * @brief Reads a script file and runs it in the sandbox.
	 *
	 * The file's path is used as the chunk name, so error messages read
	 * "path:line: message". Bytecode is rejected.
	 *
	 * @param path Path to a text script file.
	 * @return An error if the file cannot be read, or whatever run() reports.
	 */
	[[nodiscard]] LuaResult<void> runFile(const std::string& path);

	/**
	 * @brief Adds engine API to the script environment.
	 *
	 * @p fn receives the lua_State with the environment table at the top of the
	 * stack. Typically it calls registerFunction() to add functions. The table is
	 * popped afterward, so @p fn should leave the stack as it found it.
	 *
	 * @section usage Usage
	 * @code
	 * int add(int a, int b) {
	 *   return a + b;
	 * }
	 *
	 * LuaScript srcipt;
	 * script.registerApi([](luaState* L) {
	 *   registerFunction<&add>(L, "add");
	 * });
	 * @endcode
	 */
	template <typename F>
	void registerApi(F&& fn) {
		assertOwningThread();
		pushEnvironment();
		std::forward<F>(fn)(state);
		lua_pop(state, 1);
	}

	/**
	 * @brief Compiles and runs a text chunk in the script environment.
	 *
	 * Bytecode is rejected. Runtime errors carry a traceback.
	 * @param chunkName Name used in error messages, for example "player.lua".
	 */
	[[nodiscard]] LuaResult<void> run(std::string_view chunkName, std::string_view source);

	/**
	 * @brief Calls a global function defined by a previously run chunk.
	 *
	 * @tparam R Return type. Use void or a type with a LuaConverter specialization.
	 * @tparam Args Argument types. Each must have a LuaConverter specialization.
	 */
	template <typename R = void, typename... Args>
	[[nodiscard]] LuaResult<R> call(std::string_view name, const Args&... args);

	/// @return Bytes currently allocated by the Lua state.
	[[nodiscard]] size_t memoryUsed() const noexcept { return memBudget.used; }

private:
	/// Heap accounting shared with the Lua allocator. Must outlive state.
	struct MemoryBudget {
		size_t used = 0;
		size_t limit = 0;   ///< 0 means unlimited. Only used during setup.
	};

	static void* allocate(void* ud, void* ptr, size_t osize, size_t nsize);
	static void hook(lua_State* L, lua_Debug* ar);
	static int traceback(lua_State* L);
	static std::string errorMessage(lua_State* L);
	static int print(lua_State* L);

	void buildSandbox();
	void pushEnvironment() const;
	int protectedCall(int nargs, int nresults, int handler);
	void assertOwningThread() const noexcept;

	LuaScriptConfig config;
	MemoryBudget memBudget;
	lua_State* state = nullptr;
	int envRef = LUA_NOREF; ///< Registry reference to the environment table.
	std::thread::id owningThread;
	std::chrono::steady_clock::time_point deadline{};
	int activeCalls = 0; ///< Depth of protected calls in progress.
};

template <typename R, typename... Args>
LuaResult<R> LuaScript::call(std::string_view name, const Args&... args) {
	assertOwningThread();

	const int top = lua_gettop(state);
	if (!lua_checkstack(state, static_cast<int>(sizeof...(Args)) + 4))
		return LuaError{ "Lua stack overflow", 0 };

	lua_pushcfunction(state, traceback);
	const int handler = top + 1;

	// Look up the function with a raw get. A plain gettable could run a
	// metamethod that a script installed on its own environment, and an error
	// raised there would be unprotected.
	pushEnvironment();
	lua_pushlstring(state, name.data(), name.size());
	lua_rawget(state, -2);
	lua_remove(state, -2);

	if (!lua_isfunction(state, -1)) {
		lua_settop(state, top);
		return LuaError{ "'" + std::string(name) + "' is not a function", 0 };
	}

	(LuaConverter<std::decay_t<Args>>::push(state, args), ...);

	const int resultCount = std::is_void_v<R> ? 0 : 1;
	const int status = protectedCall(static_cast<int>(sizeof...(Args)), resultCount, handler);

	if (status != LUA_OK) {
		LuaError error{ errorMessage(state), 0 };
		lua_settop(state, top);
		return error;
	}

	if constexpr (std::is_void_v<R>) {
		lua_settop(state, top);
		return {};
	} else {
		LuaResult<R> result = LuaConverter<R>::fromLua(state, -1);
		lua_settop(state, top);
		return result;
	}

}

} // namespace Blackthorn::Scripting