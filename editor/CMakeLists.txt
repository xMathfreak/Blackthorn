cmake_minimum_required(VERSION 3.16.0)
project(BlackthornEditor LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE EDITOR_HEADERS CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

add_executable(BlackthornEditor
	${EDITOR_SOURCES}
	${EDITOR_HEADERS}
)

target_link_libraries(BlackthornEditor
	PRIVATE
		BlackthornEngine
		imgui
)

target_include_directories(BlackthornEditor
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/include
)