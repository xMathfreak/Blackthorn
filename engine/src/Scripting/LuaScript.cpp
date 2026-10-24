#include "Scripting/LuaScript.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>

#include <lualib.h>

namespace Blackthorn::Scripting {

namespace {

/// Reached only when an error is raised with no protected call active.
/// Nothing can recover from that, so report it and stop.
int panicHandler(lua_State* L) {
	std::fprintf(stderr, "Unprotected Lua error: %s\n", lua_tostring(L, -1));
	std::abort();
}

} // namespace

void* LuaScript::allocate(void* ud, void* ptr, size_t osize, size_t nsize) {
	auto* budget = static_cast<MemoryBudget*>(ud);

	// Lua passes a type tag, not a size, in osize when ptr is null.
	const size_t oldSize = ptr ? osize : 0;

	if (nsize == 0) {
		std::free(ptr);
		budget->used -= oldSize;
		return nullptr;
	}

	const size_t next = budget->used - oldSize + nsize;
	if (budget->limit != 0 && next > budget->limit)
		return nullptr;   // Lua memory error.

	void* result = std::realloc(ptr, nsize);
	if (result)
		budget->used = next;
	return result;
}

void LuaScript::hook(lua_State* L, lua_Debug*) {
	auto* self = *static_cast<LuaScript**>(lua_getextraspace(L));

	// Coroutines created during a call inherit the hook. Ignore it outside calls.
	if (self->activeCalls == 0)
		return;

	if (std::chrono::steady_clock::now() > self->deadline)
		luaL_error(L, "script exceeded its time slice");
}

int LuaScript::traceback(lua_State* L) {
	const char* msg = lua_tostring(L, 1);   // may be null for non-string errors
	luaL_traceback(L, L, msg, 1);
	return 1;
}

std::string LuaScript::errorMessage(lua_State* L) {
	const char* s = lua_tostring(L, -1);
	return s ? s : "non-string Lua error";
}

LuaScript::LuaScript(LuaScriptConfig cfg)
	: config(cfg), owningThread(std::this_thread::get_id()) {
	// The budget has no limit during setup, so setup cannot fail on memory.
	state = lua_newstate(allocate, &memBudget, 0);
	if (!state)
		throw std::runtime_error("Failed to create Lua state");

	// Store `this` in the per-thread extra space. Coroutines copy this space
	// when created, so the hook can find the owner from any thread.
	*static_cast<LuaScript**>(lua_getextraspace(state)) = this;

	lua_atpanic(state, panicHandler);
	luaL_openlibs(state);
	buildSandbox();

	memBudget.limit = config.memoryLimitBytes;
}

LuaScript::~LuaScript() {
	assertOwningThread();
	if (state)
		lua_close(state);
}

void LuaScript::buildSandbox() {
	// Remove unsafe fields from the real libraries. Scripts reach these tables
	// through the environment, and so does the string metatable's __index.
	lua_getglobal(state, "string");
	lua_pushnil(state);
	lua_setfield(state, -2, "dump");
	lua_pop(state, 1);

	static constexpr const char* allowedGlobals[] = {
		"assert", "error", "pcall", "xpcall", "select",
		"tostring", "tonumber", "type", "ipairs", "pairs", "next",
		"rawget", "rawset", "rawequal", "rawlen",
		"setmetatable", "_VERSION"
	};
	static constexpr const char* allowedLibraries[] = {
		"string", "table", "math", "utf8"
	};

	lua_createtable(state, 0, 32);

	for (const char* name : allowedGlobals) {
		lua_getglobal(state, name);
		lua_setfield(state, -2, name);
	}

	lua_pushcfunction(state, print);
	lua_setfield(state, -2, "print");

	for (const char* name : allowedLibraries) {
		lua_getglobal(state, name);
		lua_setfield(state, -2, name);
	}

	// Not whitelisted, so absent from the environment: load, loadfile, dofile,
	// require, package, io, os, debug, coroutine, collectgarbage, getmetatable.
	// getmetatable is excluded because it can reach the string metatable.

	envRef = luaL_ref(state, LUA_REGISTRYINDEX);
}

void LuaScript::pushEnvironment() const {
	lua_rawgeti(state, LUA_REGISTRYINDEX, envRef);
}

void LuaScript::assertOwningThread() const noexcept {
	assert(std::this_thread::get_id() == owningThread && "LuaScript used off its owning thread");
}

int LuaScript::protectedCall(int nargs, int nresults, int handler) {
	// Only the outermost call sets the deadline and arms the hook. Nested calls
	// (script -> engine -> script) share the outer deadline.
	if (activeCalls++ == 0) {
		deadline = std::chrono::steady_clock::now() + config.timeSlice;
		lua_sethook(state, hook, LUA_MASKCOUNT, static_cast<int>(config.hookInterval));
	}

	const int status = lua_pcall(state, nargs, nresults, handler);

	if (--activeCalls == 0)
		lua_sethook(state, nullptr, 0, 0);

	return status;
}

LuaResult<void> LuaScript::run(std::string_view chunkName, std::string_view source) {
	assertOwningThread();

	const int top = lua_gettop(state);
	lua_pushcfunction(state, traceback);
	const int handler = top + 1;

	// "=" tells Lua to use the name as given, without a source-file prefix.
	const std::string name = "=" + std::string(chunkName);
	const int loadStatus = luaL_loadbufferx(
		state, source.data(), source.size(), name.c_str(), "t"   // "t": reject bytecode
	);
	if (loadStatus != LUA_OK) {
		LuaError error{ errorMessage(state), 0 };
		lua_settop(state, top);
		return error;
	}

	// The chunk's first upvalue is _ENV. Point it at the sandbox environment.
	pushEnvironment();
	lua_setupvalue(state, -2, 1);

	const int status = protectedCall(0, 0, handler);
	if (status != LUA_OK) {
		LuaError error{ errorMessage(state), 0 };
		lua_settop(state, top);
		return error;
	}

	lua_settop(state, top);
	return {};
}

LuaResult<void> LuaScript::runFile(const std::string& path) {
	assertOwningThread();

	// Read the whole file as bytes. Text mode would alter line endings on
	// Windows and shift the line numbers that error messages report.
	std::ifstream file(path, std::ios::binary);
	if (!file)
		return LuaError{ "cannot open script file: " + path, 0 };

	std::string source((std::istreambuf_iterator<char>(file)),
		std::istreambuf_iterator<char>());
	if (file.bad())
		return LuaError{ "failed to read script file: " + path, 0 };

	return run(path, source);
}

int LuaScript::print(lua_State* L) {
	auto* self = *static_cast<LuaScript**>(lua_getextraspace(L));
	const int argc = lua_gettop(L);

	// Lua's print separates arguments with tabs and ends with a newline.
	// The line is built in C++ storage, so it is bounded by maxPrintBytes.
	std::string line;
	for (int i = 1; i <= argc; ++i) {
		if (i > 1)
			line += '\t';

		// luaL_tolstring applies __tostring, and formats numbers the same
		// way tostring does. It pushes one string, which we pop after copying.
		size_t length = 0;
		const char* text = luaL_tolstring(L, i, &length);
		if (line.size() < self->config.maxPrintBytes)
			line.append(text, length);
		lua_pop(L, 1);
	}

	if (line.size() > self->config.maxPrintBytes) {
		line.resize(self->config.maxPrintBytes);
		line += "...";
	}

	if (self->config.printSink) {
		self->config.printSink(line);
	} else {
		std::fwrite(line.data(), 1, line.size(), stdout);
		std::fputc('\n', stdout);
	}
	return 0;
}

} // namespace Blackthorn::Scripting