cmake_minimum_required(VERSION 3.16.0)
project(App VERSION 0.1.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE APP_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE APP_HEADERS
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

add_executable(${PROJECT_NAME}
	${APP_SOURCES}
	${APP_HEADERS}
)

enable_windows_gui(${PROJECT_NAME})

target_compile_definitions(${PROJECT_NAME}
	PRIVATE
		$<$<CONFIG:Debug>:BLACKTHORN_DEBUG>
		$<$<NOT:$<CONFIG:Debug>>:BLACKTHORN_RELEASE>
)

target_compile_options(${PROJECT_NAME} PRIVATE
	# MSVC
	$<$<CXX_COMPILER_ID:MSVC>:
		/W4
		/permissive-
		/wd4100
		/wd4201
		/wd4324

		$<$<CONFIG:Debug>:
			/WX
			/Zi
			/GS
		>

		$<$<CONFIG:RelWithDebInfo>:
			/O2
			/Zi
		>

		$<$<CONFIG:Release>:
			/O2
			/GL
		>
	>

	# GCC/Clang
	$<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
		-Wall
		-Wextra
		-Wpedantic
		-Wshadow
		-Wduplicated-cond
		-Wnull-dereference
		-Wno-unused-parameter
		-Wimplicit-fallthrough
		-fvisibility=hidden
		-Wformat=2

		$<$<CONFIG:Debug>:
			-Og
			-ggdb
			-Werror
			-fstack-protector-strong
		>

		$<$<CONFIG:RelWithDebInfo>:
			-O2
			-g
		>

		$<$<CONFIG:Release>:
			-O3
			-mtune=native
		>
	>
)

target_include_directories(${PROJECT_NAME}
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
	PRIVATE
		BlackthornEngine
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_SOURCE_DIR}/assets
	${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets
)

set_target_properties(${PROJECT_NAME} PROPERTIES
	OUTPUT_NAME "Game"
)