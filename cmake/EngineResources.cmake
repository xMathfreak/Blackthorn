set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

# -----------------------------------------------------------------------------
# Resource symbols
# -----------------------------------------------------------------------------

# Generates a stable C++ symbol from a resource's virtual path.
# RESOURCE_PATH must be relative to CMAKE_CURRENT_SOURCE_DIR.
function(engine_resource_symbol RESOURCE_PATH OUT_VAR)
	get_filename_component(NAME_ONLY "${RESOURCE_PATH}" NAME_WE)
	string(MAKE_C_IDENTIFIER "${NAME_ONLY}" IDENT)

	string(SHA256 HASH "${RESOURCE_PATH}")
	string(SUBSTRING "${HASH}" 0 12 HASH)

	set(${OUT_VAR} "bt_res_${IDENT}_${HASH}" PARENT_SCOPE)
endfunction()


# -----------------------------------------------------------------------------
# Resource embedding
# -----------------------------------------------------------------------------

function(embed_resource TARGET RESOURCE)
	if(NOT TARGET "${TARGET}")
		message(FATAL_ERROR
			"embed_resource: target '${TARGET}' does not exist"
		)
	endif()

	get_filename_component(RESOURCE_ABSOLUTE
		"${RESOURCE}"
		ABSOLUTE
		BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
	)

	if(NOT EXISTS "${RESOURCE_ABSOLUTE}")
		message(FATAL_ERROR
			"embed_resource: resource does not exist: ${RESOURCE}"
		)
	endif()

	# Resolve the stable virtual resource path.
	file(RELATIVE_PATH RESOURCE_PATH
		"${CMAKE_CURRENT_SOURCE_DIR}"
		"${RESOURCE_ABSOLUTE}"
	)
	file(TO_CMAKE_PATH "${RESOURCE_PATH}" RESOURCE_PATH)

	engine_resource_symbol("${RESOURCE_PATH}" RESOURCE_SYMBOL)

	set(OUTPUT_DIR "${GEN_DIR}/${TARGET}/include/Resources")
	set(OUTPUT_BASE "${OUTPUT_DIR}/${RESOURCE_SYMBOL}")

	set(SOURCE_OUTPUT "${OUTPUT_BASE}.cpp")
	set(HEADER_OUTPUT "${OUTPUT_BASE}.h")

	add_custom_command(
		OUTPUT
			"${SOURCE_OUTPUT}"
			"${HEADER_OUTPUT}"

		COMMAND
			"${CMAKE_COMMAND}" -E make_directory "${OUTPUT_DIR}"

		COMMAND
			"$<TARGET_FILE:btgen>"
			"${RESOURCE_ABSOLUTE}"
			"${OUTPUT_BASE}"
			-s "${RESOURCE_SYMBOL}"

		DEPENDS
			btgen
			"${RESOURCE_ABSOLUTE}"

		COMMENT "Embedding resource: ${RESOURCE_PATH}"
		VERBATIM
	)

	target_sources("${TARGET}" PRIVATE
		"${SOURCE_OUTPUT}"
	)

	target_include_directories("${TARGET}" PRIVATE
		"${OUTPUT_DIR}"
	)

	# Register resource metadata for the central registry.
	set_property(TARGET "${TARGET}" APPEND PROPERTY
		ENGINE_RESOURCE_PATHS "${RESOURCE_PATH}"
	)

	set_property(TARGET "${TARGET}" APPEND PROPERTY
		ENGINE_RESOURCE_SYMBOLS "${RESOURCE_SYMBOL}"
	)
endfunction()


# -----------------------------------------------------------------------------
# Resource registry
# -----------------------------------------------------------------------------

function(generate_resource_registry TARGET)
	if(NOT TARGET "${TARGET}")
		message(FATAL_ERROR
			"generate_resource_registry: target '${TARGET}' does not exist"
		)
	endif()

	get_target_property(PATHS "${TARGET}" ENGINE_RESOURCE_PATHS)

	if(NOT PATHS)
		set(PATHS "")
	endif()

	# Duplicate resource paths are an error.
	list(LENGTH PATHS RAW_COUNT)
	list(REMOVE_DUPLICATES PATHS)
	list(LENGTH PATHS UNIQUE_COUNT)

	if(NOT RAW_COUNT EQUAL UNIQUE_COUNT)
		message(FATAL_ERROR
			"${TARGET}: duplicate embedded resource paths"
		)
	endif()

	# Keep ordering consistent with strcmp() on the C++ side.
	list(SORT PATHS COMPARE STRING CASE SENSITIVE)

	set(DECLS "")
	set(TABLE "")

	foreach(PATH IN LISTS PATHS)
		engine_resource_symbol("${PATH}" SYMBOL)

		string(APPEND DECLS
			"extern const unsigned char ${SYMBOL}[];\n"
			"extern const std::size_t ${SYMBOL}_size;\n"
		)

		string(APPEND TABLE
			"\t{\"${PATH}\", ${SYMBOL}, ${SYMBOL}_size},\n"
		)
	endforeach()

	list(LENGTH PATHS COUNT)

	if(COUNT EQUAL 0)
		set(TABLE "\t{nullptr, nullptr, 0},\n")
	endif()

	set(REGISTRY_SOURCE "${GEN_DIR}/${TARGET}/ResourceRegistry.cpp")

	file(MAKE_DIRECTORY "${GEN_DIR}/${TARGET}")

	configure_file(
		"${BLACKTHORN_RESOURCES_DIR}/ResourceRegistry.cpp.in"
		"${REGISTRY_SOURCE}"
		@ONLY
	)

	target_sources("${TARGET}" PRIVATE
		"${REGISTRY_SOURCE}"
	)
endfunction()
