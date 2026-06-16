cmake_minimum_required(VERSION 3.16.0)
project(BlackthornEngine VERSION 0.1.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source Directories
file(GLOB_RECURSE ALL_ENGINE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE ALL_ENGINE_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
file(GLOB_RECURSE ALL_ENGINE_PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/private/*.h")

file(GLOB_RECURSE AUDIO_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/Audio/*.cpp")
file(GLOB_RECURSE GRAPHICS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/*.cpp")
file(GLOB_RECURSE FONT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/Fonts/*.cpp")
file(GLOB_RECURSE UI_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/UI/*.cpp")

# Client only sources (Excluded from Core)
set(CLIENT_ONLY_SOURCES
	# Engine graphics entry point
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Core/Engine.cpp"

	${AUDIO_SOURCES}
	${GRAPHICS_SOURCES}
	${FONT_SOURCES}
	${UI_SOURCES}
)

# Core sources = all sources minus client-only ones.
set(CORE_SOURCES ${ALL_ENGINE_SOURCES})
foreach(src ${CLIENT_ONLY_SOURCES})
	list(REMOVE_ITEM CORE_SOURCES "${src}")
endforeach()

# Simulation only (headless)
add_library(BlackthornCore SHARED
	${CORE_SOURCES}
	${ALL_ENGINE_HEADERS}
	${ALL_ENGINE_PRIVATE}
)

target_compile_definitions(BlackthornCore
	PRIVATE
		BLACKTHORN_EXPORTS
		BLACKTHORN_HEADLESS

	PUBLIC
		$<$<CONFIG:Debug>:BLACKTHORN_DEBUG>
		$<$<NOT:$<CONFIG:Debug>>:BLACKTHORN_RELEASE>

		BLACKTHORN_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
		BLACKTHORN_VERSION_MINOR=${PROJECT_VERSION_MINOR}
		BLACKTHORN_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

target_include_directories(BlackthornCore
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src
		${CMAKE_CURRENT_SOURCE_DIR}/private
)

target_link_libraries(BlackthornCore
	PUBLIC
		glm::glm
		SDL3::SDL3
		libzstd_static
		sodium
)

# Transport layer requires WinSock2 on Windows.
if(WIN32)
	target_link_libraries(BlackthornCore PUBLIC ws2_32 Avrt)
endif()

# Full client build
add_library(BlackthornEngine SHARED
	${CLIENT_ONLY_SOURCES}
	${ALL_ENGINE_HEADERS}
	${ALL_ENGINE_PRIVATE}
)

target_compile_definitions(BlackthornEngine
	PRIVATE
		BLACKTHORN_EXPORTS
		BLACKTHORN_CLIENT

	PUBLIC
		$<$<CONFIG:Debug>:BLACKTHORN_DEBUG>
		$<$<NOT:$<CONFIG:Debug>>:BLACKTHORN_RELEASE>
)

target_include_directories(BlackthornEngine
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src
		${CMAKE_CURRENT_SOURCE_DIR}/private
)

target_link_libraries(BlackthornEngine
	PUBLIC
		BlackthornCore
		SDL3_image::SDL3_image
		SDL3_ttf::SDL3_ttf
		glad
		glfw
		dr_libs
		vorbis
		vorbisfile
		ogg
		OpenAL::OpenAL

	PRIVATE
		OpenGL::GL
)

# Shared compiler options
foreach(TARGET BlackthornCore BlackthornEngine)
	target_compile_definitions(${TARGET} PUBLIC GLM_FORCE_INTRINSICS)

	target_compile_options(${TARGET} PRIVATE
		# MSVC
		$<$<CXX_COMPILER_ID:MSVC>:
			/W4
			/permissive-
			/wd4100
			/wd4201
			/wd4324

			$<$<CONFIG:Debug>:
				/WX
				/Zi
				/GS
			>

			$<$<CONFIG:RelWithDebInfo>:
				/O2
				/Zi
			>

			$<$<CONFIG:Release>:
				/O2
				/GL
			>
		>

		# GCC/Clang
		$<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:
			-Wall
			-Wextra
			-Wpedantic
			-Wshadow
			-Wnull-dereference
			-Wno-unused-parameter
			-Wimplicit-fallthrough
			-fvisibility=hidden
			-Wformat=2

			$<$<CONFIG:Debug>:
				-Og
				-ggdb
				-Werror
				-fstack-protector-strong
			>

			$<$<CONFIG:RelWithDebInfo>:
				-O2
				-g
			>

			$<$<CONFIG:Release>:
				-O3
				-mtune=native
			>
		>
	)

	set_target_properties(${TARGET} PROPERTIES
		PREFIX                    ""
		DEBUG_POSTFIX             "d"
		LINKER_LANGUAGE           CXX
		RUNTIME_OUTPUT_DIRECTORY  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
		LIBRARY_OUTPUT_DIRECTORY  ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
		ARCHIVE_OUTPUT_DIRECTORY  ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
		POSITION_INDEPENDENT_CODE ON
	)

	if (IPO_SUPPORTED)
		set_target_properties(${TARGET} PROPERTIES
			INTERPROCEDURAL_OPTIMIZATION $<$<CONFIG:Release>:TRUE>
		)
	endif()
endforeach()

# Windows dll copy
if(WIN32)
	foreach(LIB BlackthornEngine SDL3::SDL3 SDL3_image::SDL3_image SDL3_ttf::SDL3_ttf)
		add_custom_command(TARGET BlackthornEngine POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				$<TARGET_FILE:${LIB}>
				${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
		)
	endforeach()
endif()