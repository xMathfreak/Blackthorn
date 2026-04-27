cmake_minimum_required(VERSION 3.16.0)

# glm
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
add_subdirectory(glm)

# glfw
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(glfw)

# glad
add_library(glad STATIC
	${CMAKE_CURRENT_SOURCE_DIR}/glad/src/glad.c
	${CMAKE_CURRENT_SOURCE_DIR}/glad/include/glad/glad.h
	${CMAKE_CURRENT_SOURCE_DIR}/glad/include/KHR/khrplatform.h
)

target_include_directories(glad
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/glad/include
		${CMAKE_CURRENT_SOURCE_DIR}/glm
)

set_target_properties(glad PROPERTIES LINKER_LANGUAGE C)

# zstd
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_INSTALL OFF CACHE BOOL "")
add_subdirectory(zstd)

# libsodium
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SODIUM_DISABLE_TESTS ON CACHE BOOL "" FORCE)
add_subdirectory(libsodium)