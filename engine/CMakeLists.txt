cmake_minimum_required(VERSION 3.16.0)
project(BlackthornEngine VERSION 0.1.0 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Source discovery
# ---------------------------------------------------------------------------

file(GLOB_RECURSE ALL_ENGINE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE ALL_ENGINE_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
file(GLOB_RECURSE ALL_ENGINE_PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/private/*.h")

# ---------------------------------------------------------------------------
# Client-only translation units
# Excluded from BlackthornEngineCore. Any file that includes glad, OpenGL,
# SDL3_image, SDL3_ttf, or UIManager belongs here.
# ---------------------------------------------------------------------------

set(CLIENT_ONLY_SOURCES
	# Engine graphics entry point
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Core/Engine.cpp"

	# Rendering
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/EBO.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/FBO.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Renderer.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Shader.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/Texture.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/VAO.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Graphics/VBO.cpp"

	# Fonts (TTF/bitmap - depend on SDL3_ttf and OpenGL)
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Fonts/BitmapFont.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Fonts/TrueTypeFont.cpp"

	# UI
	"${CMAKE_CURRENT_SOURCE_DIR}/src/UI/Container.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/UI/UIManager.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/UI/Widget.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/UI/Widgets/Label.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/UI/Widgets/Panel.cpp"
)

# Core sources = all sources minus client-only ones.
set(CORE_SOURCES ${ALL_ENGINE_SOURCES})
foreach(src ${CLIENT_ONLY_SOURCES})
	list(REMOVE_ITEM CORE_SOURCES "${src}")
endforeach()

# ---------------------------------------------------------------------------
# BlackthornEngineCore - simulation only
#
# No SDL3_image, no SDL3_ttf, no glad, no OpenGL, no UIManager.
# Server binaries link only this target.
# ---------------------------------------------------------------------------

add_library(BlackthornEngineCore SHARED
	${CORE_SOURCES}
	${ALL_ENGINE_HEADERS}
	${ALL_ENGINE_PRIVATE}
)

target_compile_definitions(BlackthornEngineCore
	PRIVATE
		BLACKTHORN_EXPORTS
		BLACKTHORN_HEADLESS

	PUBLIC
		$<$<CONFIG:Debug>:BLACKTHORN_DEBUG>
		$<$<CONFIG:Release>:BLACKTHORN_RELEASE>
		BLACKTHORN_HEADLESS
)

target_include_directories(BlackthornEngineCore
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src
		${CMAKE_CURRENT_SOURCE_DIR}/private
)

target_link_libraries(BlackthornEngineCore
	PUBLIC
		glm::glm
		SDL3::SDL3
)

set_target_properties(BlackthornEngineCore PROPERTIES
	CXX_VISIBILITY_PRESET hidden
)

# Transport layer requires WinSock2 on Windows.
if(WIN32)
	target_link_libraries(BlackthornEngineCore PUBLIC ws2_32)
endif()

# ---------------------------------------------------------------------------
# BlackthornEngine - full client build
#
# Links BlackthornEngineCore and adds all graphics-related dependencies.
# Client binaries link this target (which transitively brings in Core).
# ---------------------------------------------------------------------------

add_library(BlackthornEngine SHARED
	${CLIENT_ONLY_SOURCES}
	${ALL_ENGINE_HEADERS}
	${ALL_ENGINE_PRIVATE}
)

target_compile_definitions(BlackthornEngine
	PRIVATE
		BLACKTHORN_EXPORTS

	PUBLIC
		$<$<CONFIG:Debug>:BLACKTHORN_DEBUG>
		$<$<CONFIG:Release>:BLACKTHORN_RELEASE>
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
		BlackthornEngineCore
		glm::glm
		SDL3::SDL3
		SDL3_image::SDL3_image
		SDL3_ttf::SDL3_ttf
		glad
		glfw

	PRIVATE
		OpenGL::GL
)

# ---------------------------------------------------------------------------
# Shared compiler options
# ---------------------------------------------------------------------------

foreach(TARGET BlackthornEngineCore BlackthornEngine)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
		target_compile_definitions(${TARGET} PUBLIC
			GLM_FORCE_SIMD_AVX
			GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
		)
		target_compile_options(${TARGET} PRIVATE -mavx)

	elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i686|i386|x86")
		target_compile_definitions(${TARGET} PUBLIC
			GLM_FORCE_SIMD_SSE2
			GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
		)
		target_compile_options(${TARGET} PRIVATE -msse2)

	else()
		target_compile_definitions(${TARGET} PUBLIC GLM_FORCE_INTRINSICS)
	endif()

	target_compile_options(${TARGET} PRIVATE
		-Wall -Wextra -Wpedantic
		-Wno-unused-parameter
		-Wshadow
		-Wduplicated-cond
	)

	set_target_properties(${TARGET} PROPERTIES
		DEBUG_POSTFIX            "d"
		LINKER_LANGUAGE          CXX
		RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
		LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
		ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
	)

	if(CMAKE_BUILD_TYPE STREQUAL "Release" AND CMAKE_INTERPROCEDURAL_OPTIMIZATION)
		set_target_properties(${TARGET} PROPERTIES
			INTERPROCEDURAL_OPTIMIZATION TRUE
		)
	endif()
endforeach()

# ---------------------------------------------------------------------------
# Windows DLL copy - client target only
# ---------------------------------------------------------------------------

if(WIN32)
	foreach(LIB BlackthornEngine SDL3::SDL3 SDL3_image::SDL3_image SDL3_ttf::SDL3_ttf)
		add_custom_command(TARGET BlackthornEngine POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
				$<TARGET_FILE:${LIB}>
				${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
		)
	endforeach()
endif()