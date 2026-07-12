option(BLACKTHORN_BUILD_ASSET_PACKER "Build btpacker" ON)

if(BLACKTHORN_BUILD_ASSET_PACKER)
	add_subdirectory(btpacker)
endif()

if(TARGET btpacker)
	set(MANIFESTS_DIR "${PROJECT_SOURCE_DIR}/data")

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

		WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
	)
endif()