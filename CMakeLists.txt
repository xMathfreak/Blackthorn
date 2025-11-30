cmake_minimum_required(VERSION 3.16.0)
project(Blackthorn VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Debug, Release)" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG -g3 -ggdb -O0 -Wall -Wextra -Werror -fstack-protector-all")
set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O3 -flto -fwhole-program -march=native -funroll-loops -fomit-frame-pointer -ftree-vectorize -fopenmp -flax-vector-conversions")

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	add_compile_options(-fsanitize=address -fsanitize=undefined)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
	include(CheckIPOSupported)
	check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)

	if(ipo_supported)
		message(STATUS "IPO/LTO enabled for Release build")
		set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
	else()
		message(STATUS "IPO/LTO not supported: ${ipo_error}")
	endif()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(BLACKTHORN_BUILD_APP "Build sample application" ON)

find_package(OpenGL REQUIRED)
find_package(SDL3 REQUIRED)
find_package(SDL3_image REQUIRED)

add_subdirectory(third_party)
add_subdirectory(engine)

if (BLACKTHORN_BUILD_APP)
	add_subdirectory(app)
endif()