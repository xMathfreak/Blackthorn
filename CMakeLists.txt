cmake_minimum_required(VERSION 3.16.0)
project(Blackthorn VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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