option(BLACKTHORN_BUILD_ASSET_PACKER "Build Asset Packer" ON)
option(BLACKTHORN_BUILD_FONT_PACKER "Build Font Packer" ON)

add_library(BlackthornCLI STATIC
	include/CLI.h
	src/CLI.cpp
)

target_include_directories(BlackthornCLI
	PUBLIC
		include
)

target_compile_features(BlackthornCLI PUBLIC cxx_std_20)

if(BLACKTHORN_BUILD_ASSET_PACKER)
	add_subdirectory(btpacker)
endif()

if(BLACKTHORN_BUILD_ASSET_PACKER)
	add_subdirectory(btfpacker)
endif()

if(TARGET btpacker)
	set(MANIFESTS_DIR "${CMAKE_SOURCE_DIR}/data")

	add_custom_target(pack_assets
		COMMENT "Packing game assets..."

		DEPENDS btpacker

		COMMAND $<TARGET_FILE:btpacker>
			pack
			--manifest "${MANIFESTS_DIR}/base.pack.json"

		# Add more manifests as needed:
		# COMMAND $<TARGET_FILE:btpacker>
		#     pack
		#     --manifest "${MANIFESTS_DIR}/chapter1.pack.json"

		WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
	)
endif()