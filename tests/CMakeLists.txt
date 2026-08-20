cmake_minimum_required(VERSION 3.16.0)
project(BlackthornTests VERSION 0.1.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}
	main.cpp
)

target_include_directories(${PROJECT_NAME}
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
	PRIVATE
		BlackthornCore
		BlackthornEngine
)

add_test(
	NAME BlackthornTests
	COMMAND BlackthornTests
)