cmake_minimum_required(VERSION 3.16.0)
project(BlackthornEngine VERSION 0.1.0)
project(Blackthorn LANGUAGES CXX)

file(GLOB_RECURSE ENGINE_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE ENGINE_HEADERS
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

file(GLOB_RECURSE ENGINE_PRIVATE_HEADERS
	"${CMAKE_CURRENT_SOURCE_DIR}/private/*.h"
)

add_library(${PROJECT_NAME} SHARED
	${ENGINE_SOURCES}
	${ENGINE_HEADERS}
	${ENGINE_PRIVATE_HEADERS}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE BLACKTHORN_EXPORTS)

target_include_directories(${PROJECT_NAME}
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>

	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src
		${CMAKE_CURRENT_SOURCE_DIR}/private
)

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		glm::glm
		SDL3::SDL3
    	SDL3_image::SDL3_image
		glad
		glfw
		OpenGL::GL
)

if (WIN32)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		$<TARGET_FILE:${PROJECT_NAME}>
		${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
	)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
	OUTPUT_NAME "BlackthornEngine"
	# DEBUG_POSTFIX "d"
	PREFIX ""
)