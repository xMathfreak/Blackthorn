cmake_minimum_required(VERSION 3.16.0)
project(btlocc VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(btlocc
	src/main.cpp
	src/LocManifest.h
	src/LocParser.h
	src/LocParser.cpp
	src/Compiler.h
	src/Compiler.cpp
)

target_set_metadata(btlocc
	NAME       "Blackthorn Localization Compiler"
	VERSION    "${PROJECT_VERSION}"
	IDENTIFIER "com.blackthorn.tools.localizationCompiler"
	COMPANY    ""
	COPYRIGHT  ""
	TYPE       "application"
)
target_add_platform_resources(btlocc)

target_include_directories(btlocc
	PRIVATE
		src
		${CMAKE_SOURCE_DIR}/engine/include
		${CMAKE_SOURCE_DIR}/third_party/zstd/lib/common
)

target_link_libraries(btlocc
	PRIVATE
		BlackthornCLI
		libzstd_static
)

target_compile_options(btlocc PRIVATE
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

set_target_properties(btlocc PROPERTIES
	OUTPUT_NAME              "btlocc"
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

if(IPO_SUPPORTED)
	set_target_properties(btlocc PROPERTIES
		INTERPROCEDURAL_OPTIMIZATION $<$<CONFIG:Release>:TRUE>
	)
endif()
