cmake_minimum_required(VERSION 3.20.0)

project(BlackthornEngine
	VERSION 0.1.0
	LANGUAGES CXX
)

set(CMAKE_CXX_EXTENSIONS OFF)

option (BT_PACK_MODE
	"Enable pack-file asset resolution (forced ON for Release)"
	OFF
)

set(XXHASH_INCLUDE_DIR
	"${CMAKE_SOURCE_DIR}/third_party/zstd/lib/common"
	CACHE PATH "Path to the directory containing xxhash.h"
)

set(ENGINE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(ENGINE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(ENGINE_PRIVATE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/private")

file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
	"${ENGINE_SOURCE_DIR}/*.cpp"
)

file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS
	"${ENGINE_INCLUDE_DIR}/*.hpp"
)

file(GLOB_RECURSE ENGINE_PRIVATE_HEADERS CONFIGURE_DEPENDS
	"${ENGINE_PRIVATE_DIR}/*.hpp"
)

set(CLIENT_SOURCE_DIRS
	Audio
	Graphics
	Particles
	Fonts
	UI
)

set(CLIENT_ONLY_SOURCES
	"${ENGINE_SOURCE_DIR}/Core/Engine.cpp"
)

foreach(DIR IN LISTS CLIENT_SOURCE_DIRS)
	file(GLOB_RECURSE DIR_SOURCES CONFIGURE_DEPENDS
		"${ENGINE_SOURCE_DIR}/${DIR}/*.cpp"
	)

	list(APPEND CLIENT_ONLY_SOURCES ${DIR_SOURCES})
endforeach()

set(CORE_SOURCES ${ENGINE_SOURCES})

foreach(SOURCE IN LISTS CLIENT_ONLY_SOURCES)
	list(REMOVE_ITEM CORE_SOURCES "${SOURCE}")
endforeach()


add_library(BlackthornBuildOptions INTERFACE)
add_library(Blackthorn::BuildOptions ALIAS BlackthornBuildOptions)

target_compile_features(BlackthornBuildOptions
	INTERFACE
		cxx_std_23
)

target_compile_definitions(BlackthornBuildOptions
	INTERFACE
		GLM_FORCE_INTRINSICS
)

add_library(BlackthornCore SHARED
	${CORE_SOURCES}
	${ENGINE_HEADERS}
	${ENGINE_PRIVATE_HEADERS}
)

add_library(Blackthorn::Core ALIAS BlackthornCore)

add_library(BlackthornEngine SHARED
	${CLIENT_ONLY_SOURCES}
	${ENGINE_HEADERS}
	${ENGINE_PRIVATE_HEADERS}
)

add_library(Blackthorn::Engine ALIAS BlackthornEngine)

function(blackthorn_configure_target TARGET)
	target_link_libraries(${TARGET} PUBLIC
		BlackthornBuildOptions
	)

	target_compile_definitions(${TARGET}
		PRIVATE
			BLACKTHORN_EXPORTS

		PUBLIC
			$<$<CONFIG:Debug>:BLACKTHORN_DEBUG>
			$<$<NOT:$<CONFIG:Debug>>:BLACKTHORN_RELEASE>
			$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:BT_PACK_MODE>
			$<$<AND:$<CONFIG:Debug>,$<BOOL:${BT_PACK_MODE}>>:BT_PACK_MODE>
	)

	target_include_directories(${TARGET}
		PUBLIC
			$<BUILD_INTERFACE:${ENGINE_INCLUDE_DIR}>
			$<INSTALL_INTERFACE:include>

		PRIVATE
			${ENGINE_PRIVATE_DIR}
			${ENGINE_SOURCE_DIR}
			${XXHASH_INCLUDE_DIR}
	)

	target_compile_options(${TARGET} PRIVATE
		# MSVC
		$<$<CXX_COMPILER_ID:MSVC>:
			/W4
			/permissive-
			/wd4100
			/wd4201
			/wd4324
			/arch:SSE4.2
		>

		$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:
			/WX
			/Zi
			/GS
		>

		$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:RelWithDebInfo>>:
			/O2
			/Zi
		>

		$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:
			/O2
			/GL
		>

		# GCC / Clang
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
			-msse4.2
		>

		$<$<AND:$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>,$<CONFIG:Debug>>:
			-Og
			-ggdb
			-Werror
			-fstack-protector-strong
		>

		$<$<AND:$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>,$<CONFIG:RelWithDebInfo>>:
			-O2
			-g
		>

		$<$<AND:$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>,$<CONFIG:Release>>:
			-O3
			-mtune=generic
		>
	)

	set_target_properties(${TARGET} PROPERTIES
		PREFIX ""
		DEBUG_POSTFIX "d"
		LINKER_LANGUAGE CXX
		POSITION_INDEPENDENT_CODE ON

		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
		ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}"
	)

	if(IPO_SUPPORTED)
		set_property(TARGET ${TARGET} PROPERTY
			INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
		)
	endif()
endfunction()

blackthorn_configure_target(BlackthornCore)
blackthorn_configure_target(BlackthornEngine)

target_compile_definitions(BlackthornCore PUBLIC
	BLACKTHORN_HEADLESS
	BLACKTHORN_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
	BLACKTHORN_VERSION_MINOR=${PROJECT_VERSION_MINOR}
	BLACKTHORN_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

target_link_libraries(BlackthornCore
	PUBLIC
		nlohmann_json
		glm::glm
		SDL3::SDL3
		libzstd_static
		sodium
		lua::lua
		tomlplusplus_tomlplusplus
		BlackthornShared
)

if(WIN32)
	target_link_libraries(BlackthornCore PUBLIC
		ws2_32
		Avrt
	)
endif()

target_compile_definitions(BlackthornEngine PUBLIC
	BLACKTHORN_CLIENT
)

target_include_directories(BlackthornEngine PUBLIC
	$<BUILD_INTERFACE:$<TARGET_PROPERTY:glad,INTERFACE_INCLUDE_DIRECTORIES>>
)

target_link_libraries(BlackthornEngine
	PUBLIC
		BlackthornCore
		SDL3_image::SDL3_image
		SDL3_ttf::SDL3_ttf
		dr_libs
		vorbis
		vorbisfile
		ogg
		OpenAL::OpenAL

	PRIVATE
		glad
		OpenGL::GL
)

if(WIN32)
	set(WINDOWS_RUNTIME_TARGETS
		BlackthornEngine
		SDL3::SDL3
		SDL3_image::SDL3_image
		SDL3_ttf::SDL3_ttf
	)

	foreach(TARGET IN LISTS WINDOWS_RUNTIME_TARGETS)
		add_custom_command(TARGET BlackthornEngine POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				"$<TARGET_FILE:${TARGET}>"
				"$<TARGET_FILE_DIR:BlackthornEngine>"
			VERBATIM
		)
	endforeach()

	# MinGW runtime dependencies.
	if(NOT MSVC)
		set(MINGW_RUNTIME_DLLS
			libstdc++-6.dll
			libgcc_s_seh-1.dll
			libwinpthread-1.dll
		)

		get_filename_component(MINGW_BIN_DIR
			"${CMAKE_CXX_COMPILER}"
			DIRECTORY
		)

		foreach(DLL IN LISTS MINGW_RUNTIME_DLLS)
			set(DLL_PATH "${MINGW_BIN_DIR}/${DLL}")

			if(EXISTS "${DLL_PATH}")
				add_custom_command(TARGET BlackthornEngine POST_BUILD
					COMMAND ${CMAKE_COMMAND} -E copy_if_different
						"${DLL_PATH}"
						"$<TARGET_FILE_DIR:BlackthornEngine>"
					VERBATIM
				)
			else()
				message(WARNING
					"MinGW runtime DLL not found: ${DLL_PATH}"
				)
			endif()
		endforeach()
	endif()
endif()

target_set_metadata(BlackthornEngine
	NAME       "Blackthorn Engine"
	VERSION    "${PROJECT_VERSION}"
	IDENTIFIER "com.blackthorn.engine"
)

target_add_platform_resources(BlackthornEngine)

target_set_metadata(BlackthornCore
	NAME       "Blackthorn Core"
	VERSION    "${PROJECT_VERSION}"
	IDENTIFIER "com.blackthorn.engine.core"
)

target_add_platform_resources(BlackthornCore)

include("${CMAKE_SOURCE_DIR}/cmake/EngineResources.cmake")

set(ENGINE_SHADER_RESOURCES
	default.vert
	default.frag
	font_bitmap.vert
	font_bitmap.frag
	font_ttf.vert
	font_ttf.frag
	particle.vert
	particle.frag
	screen.vert
	screen.frag
)

foreach(SHADER IN LISTS ENGINE_SHADER_RESOURCES)
	embed_resource(BlackthornEngine
		"${CMAKE_SOURCE_DIR}/assets/shaders/${SHADER}"
	)
endforeach()

generate_resource_registry(BlackthornEngine)