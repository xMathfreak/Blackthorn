cmake_minimum_required(VERSION 3.16.0)
project(App)

file(GLOB_RECURSE APP_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE APP_HEADERS
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

add_executable(${PROJECT_NAME}
	${APP_SOURCES}
	${APP_HEADERS}
)

target_include_directories(${PROJECT_NAME}
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_directories(${PROJECT_NAME}
	PRIVATE
		${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
)

target_link_libraries(${PROJECT_NAME}
	PRIVATE
		BlackthornEngine
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_SOURCE_DIR}/assets
	${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets
)

set_target_properties(${PROJECT_NAME} PROPERTIES
	OUTPUT_NAME "Game"
)