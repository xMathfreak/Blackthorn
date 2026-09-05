cmake_minimum_required(VERSION 3.16.0)
project(Blackthorn VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Platform metadata / packaging system
include(${CMAKE_SOURCE_DIR}/cmake/PlatformResources.cmake)

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
option(BLACKTHORN_BUILD_TOOLS "Build tools" ON)
option(BLACKTHORN_ENABLE_TESTING "Enable unit tests" OFF)

# External dependencies
find_package(OpenGL REQUIRED)
find_package(SDL3 REQUIRED)
find_package(SDL3_image REQUIRED)
find_package(SDL3_ttf REQUIRED)

# Subdirectories
add_subdirectory(third_party)
add_subdirectory(shared)
add_subdirectory(engine)

if (BLACKTHORN_ENABLE_TESTING)
	enable_testing()
	add_subdirectory(tests)
endif()

if(BLACKTHORN_BUILD_APP)
	add_subdirectory(app)
endif()

if(BLACKTHORN_BUILD_TOOLS)
	add_subdirectory(tools)
endif()