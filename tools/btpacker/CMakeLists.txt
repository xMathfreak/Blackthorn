cmake_minimum_required(VERSION 3.16.0)
project(btpacker VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(btpacker
	src/main.cpp
	src/Manifest.h
	src/ManifestParser.h
	src/ManifestParser.cpp
	src/Packer.h
	src/Packer.cpp
	src/ManifestGenerator.h
	src/ManifestGenerator.cpp
)

target_include_directories(btpacker
	PRIVATE
		src
		${CMAKE_SOURCE_DIR}/engine/include
		${CMAKE_SOURCE_DIR}/third_party/zstd/lib/common
)

target_link_libraries(btpacker
	PRIVATE
		libzstd_static
)

target_compile_definitions(btpacker
	PRIVATE
		$<$<CONFIG:Debug>:BTPACKER_DEBUG>
)

target_compile_options(btpacker PRIVATE
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

set_target_properties(btpacker PROPERTIES
	OUTPUT_NAME              "btpacker"
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

if(IPO_SUPPORTED)
	set_target_properties(btpacker PROPERTIES
		INTERPROCEDURAL_OPTIMIZATION $<$<CONFIG:Release>:TRUE>
	)
endif()