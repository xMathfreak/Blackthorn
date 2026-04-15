cmake_minimum_required(VERSION 3.16.0)

set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
add_subdirectory(glm)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(glfw)

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