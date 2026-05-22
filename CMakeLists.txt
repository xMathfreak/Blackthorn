cmake_minimum_required(VERSION 3.16.0)
project(Blackthorn VERSION 0.1.0 LANGUAGES C CXX)

# Use CCache if compile is unset
find_program(CCACHE_PROGRAM ccache)

if(CCACHE_PROGRAM)
	message(STATUS "Using ccache from: ${CCACHE_PROGRAM}")

	if(NOT DEFINED CMAKE_C_COMPILER_LAUNCHER)
		set(CMAKE_C_COMPILER_LAUNCHER "$<$<CONFIG:Debug>:${CCACHE_PROGRAM}>")
	endif()

	if(NOT DEFINED CMAKE_CXX_COMPILER_LAUNCHER)
		set(CMAKE_CXX_COMPILER_LAUNCHER "$<$<CONFIG:Debug>:${CCACHE_PROGRAM}>")
	endif()
else()
	message(STATUS "ccache not found")
endif()

# Build type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Hide console in Release mode (Windows only)
function(enable_windows_gui target)
	if(WIN32)
		target_link_options(${target} PRIVATE
			$<$<CONFIG:Release>:-mwindows>
		)
	endif()
endfunction()

# Check IPO/LTO support for release builds
include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT IPO_ERROR)
if(IPO_SUPPORTED)
	message(STATUS "IPO/LTO is supported and will be enabled for Release builds")
else()
	message(STATUS "IPO/LTO not supported: ${IPO_ERROR}")
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Options
option(BLACKTHORN_BUILD_APP "Build sample application" ON)

# External dependencies
find_package(OpenGL REQUIRED)
find_package(SDL3 REQUIRED)
find_package(SDL3_image REQUIRED)
find_package(SDL3_ttf REQUIRED)

# Subdirectories
add_subdirectory(third_party)
add_subdirectory(engine)

if (BLACKTHORN_BUILD_APP)
	add_subdirectory(app)
endif()