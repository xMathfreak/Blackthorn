cmake_minimum_required(VERSION 3.16.0)
project(App VERSION 0.1.0 LANGUAGES CXX)

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

enable_windows_gui(${PROJECT_NAME})

target_compile_definitions(${PROJECT_NAME}
	PRIVATE
		$<$<CONFIG:Debug>:BLACKTHORN_DEBUG>
		$<$<CONFIG:Release>:BLACKTHORN_RELEASE>
)

target_compile_options(${PROJECT_NAME} PRIVATE
	-Wall
	-Wextra
	-Wpedantic
	-Wno-unused-parameter
	-Wshadow
	-Wduplicated-cond
)

target_include_directories(${PROJECT_NAME}
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/include
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