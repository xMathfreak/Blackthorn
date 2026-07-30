cmake_minimum_required(VERSION 3.16.0)
project(btfpacker VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(btfpacker
	src/main.cpp
)

target_set_metadata(btfpacker
	NAME       "Blackthorn Font Packer"
	VERSION    "${PROJECT_VERSION}"
	IDENTIFIER "com.blackthorn.tools.fontPacker"
	COMPANY    ""
	COPYRIGHT  ""
	TYPE       "application"
)
target_add_platform_resources(btfpacker)

target_include_directories(btfpacker
	PRIVATE
		src
)

target_compile_definitions(btfpacker
	PRIVATE
		$<$<CONFIG:Debug>:BTFPACKER_DEBUG>
)

target_compile_options(btfpacker PRIVATE
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

set_target_properties(btfpacker PROPERTIES
	OUTPUT_NAME              "btfpacker"
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

if(IPO_SUPPORTED)
	set_target_properties(btfpacker PROPERTIES
		INTERPROCEDURAL_OPTIMIZATION $<$<CONFIG:Release>:TRUE>
	)
endif()
