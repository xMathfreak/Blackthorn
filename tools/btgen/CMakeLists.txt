cmake_minimum_required(VERSION 3.16.0)
project(btgen VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(btgen
	src/main.cpp
)

target_set_metadata(btgen
	NAME        "Blackthorn Resource Generator"
	VERSION     "${PROJECT_VERSION}"
	IDENTIFIER  "com.blackthorn.tools.resourceGenerator"
	COMPANY     ""
	COPYRIGHT   ""
	TYPE        "application"
)
target_add_platform_resources(btgen)

target_include_directories(btgen
	PRIVATE
		src
)

target_link_libraries(btgen
	PRIVATE
		BlackthornCLI
)

target_compile_definitions(btgen
	PRIVATE
		$<$<CONFIG:Debug>:BTGEN_DEBUG>
)

target_compile_options(btgen PRIVATE
	$<$<CXX_COMPILER_ID:MSVC>:
		/W4
		/permissive-
		/wd4100
		/wd4201

		$<$<CONFIG:Debug>:
			/Zi
			/GS
		>
		$<$<CONFIG:Release>:
			/O2
			/GL
		>
	>

	$<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
		-Wall
		-Wextra
		-Wpedantic
		-Wshadow
		-Wno-unused-parameter

		$<$<CONFIG:Debug>:
			-Og
			-ggdb
		>
		$<$<CONFIG:Release>:
			-O2
		>
	>
)

set_target_properties(btgen PROPERTIES
	OUTPUT_NAME              "btgen"
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/internal
)

if(IPO_SUPPORTED)
	set_target_properties(btgen PROPERTIES
		INTERPROCEDURAL_OPTIMIZATION $<$<CONFIG:Release>:TRUE>
	)
endif()
