if(NOT DEFINED LUA_SOURCE_DIR OR
   NOT EXISTS "${LUA_SOURCE_DIR}/lua.h")
    message(FATAL_ERROR
        "LUA_SOURCE_DIR must point to a valid Lua source directory."
    )
endif()

option(LUA_BUILD_COMPILER "Build the Lua compiler" OFF)
option(LUA_BUILD_INTERPRETER "Build the Lua interpreter" OFF)

set(LUA_SOURCES
    lapi.c
    lauxlib.c
    lbaselib.c
    lcode.c
    lcorolib.c
    lctype.c
    ldblib.c
    ldebug.c
    ldo.c
    ldump.c
    lfunc.c
    lgc.c
    linit.c
    liolib.c
    llex.c
    lmathlib.c
    lmem.c
    loadlib.c
    lobject.c
    lopcodes.c
    loslib.c
    lparser.c
    lstate.c
    lstring.c
    lstrlib.c
    ltable.c
    ltablib.c
    ltm.c
    lundump.c
    lutf8lib.c
    lvm.c
    lzio.c
)

# Convert source filenames into paths under the Lua submodule.
list(TRANSFORM LUA_SOURCES
    PREPEND "${LUA_SOURCE_DIR}/"
)

add_library(liblua STATIC ${LUA_SOURCES})
add_library(lua::lua ALIAS liblua)

set_source_files_properties(${LUA_SOURCES} PROPERTIES
    LANGUAGE CXX
)

target_include_directories(liblua
    PUBLIC
        $<BUILD_INTERFACE:${LUA_SOURCE_DIR}>
)

set_target_properties(liblua PROPERTIES
    EXPORT_NAME lua
    FOLDER "third_party"
)

if(UNIX AND NOT APPLE)
    include(CheckLibraryExists)
    check_library_exists(m sin "" LUA_HAVE_LIBM)

    if(LUA_HAVE_LIBM)
        target_link_libraries(liblua PUBLIC m dl)
    endif()
endif()

if(LUA_BUILD_COMPILER)
    add_executable(luac "${LUA_SOURCE_DIR}/luac.c")
    target_link_libraries(luac PRIVATE lua::lua)

    set_target_properties(luac PROPERTIES FOLDER "third_party")
endif()

if(LUA_BUILD_INTERPRETER)
    add_executable(lua "${LUA_SOURCE_DIR}/lua.c")
    target_link_libraries(lua PRIVATE lua::lua)

    set_target_properties(lua PROPERTIES FOLDER "third_party")
endif()
