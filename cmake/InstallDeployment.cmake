# ==================================================
# Blackthorn Deployment Configuration
# ==================================================

include(GNUInstallDirs)

if(POLICY CMP0207)
	cmake_policy(SET CMP0207 NEW)
endif()

# Explicit deployment directories.
set(DEPLOY_BINDIR "bin")
set(DEPLOY_LIBDIR "lib")

# Runtime dependency search directories.
set(DEPLOY_RUNTIME_DEPENDENCY_DIRECTORIES)

if(MINGW)
	get_filename_component(
		MINGW_BIN_DIR
		"${CMAKE_CXX_COMPILER}"
		DIRECTORY
	)

	list(APPEND DEPLOY_RUNTIME_DEPENDENCY_DIRECTORIES
		"${MINGW_BIN_DIR}"
	)
endif()

# --------------------------------------------------
# Configure deployment for a target.
# --------------------------------------------------

function(blackthorn_deploy TARGET_NAME)

	if(NOT TARGET "${TARGET_NAME}")
		message(FATAL_ERROR
			"Cannot deploy target '${TARGET_NAME}': target does not exist."
		)
	endif()

	get_target_property(TARGET_TYPE
		"${TARGET_NAME}"
		TYPE
	)

	# Configure platform-specific runtime paths.
	if(APPLE)

		if(TARGET_TYPE STREQUAL "EXECUTABLE")
			set_target_properties("${TARGET_NAME}" PROPERTIES
				INSTALL_RPATH "@loader_path/../lib"
			)
		elseif(TARGET_TYPE STREQUAL "SHARED_LIBRARY")
			set_target_properties("${TARGET_NAME}" PROPERTIES
				INSTALL_RPATH "@loader_path"
			)
		endif()

	elseif(UNIX)

		if(TARGET_TYPE STREQUAL "EXECUTABLE")
			set_target_properties("${TARGET_NAME}" PROPERTIES
				INSTALL_RPATH "$ORIGIN/../lib"
			)
		elseif(TARGET_TYPE STREQUAL "SHARED_LIBRARY")
			set_target_properties("${TARGET_NAME}" PROPERTIES
				INSTALL_RPATH "$ORIGIN"
			)
		endif()

	endif()

	# Install the target.
	install(
		TARGETS "${TARGET_NAME}"
		RUNTIME_DEPENDENCY_SET BlackthornRuntimeDependencies
		RUNTIME DESTINATION "${DEPLOY_BINDIR}"
		LIBRARY DESTINATION "${DEPLOY_LIBDIR}"
		ARCHIVE DESTINATION "${DEPLOY_LIBDIR}"
	)

endfunction()

# --------------------------------------------------
# Install collected third-party runtime dependencies.
# --------------------------------------------------

function(blackthorn_install_runtime_dependencies)

	install(
		RUNTIME_DEPENDENCY_SET BlackthornRuntimeDependencies

		DIRECTORIES
			${DEPLOY_RUNTIME_DEPENDENCY_DIRECTORIES}

		# Windows API-set DLLs are provided by Windows.
		PRE_EXCLUDE_REGEXES
			"api-ms-.*"
			"ext-ms-.*"

		# Exclude common operating-system libraries.
		POST_EXCLUDE_REGEXES
			# Windows system directories.
			".*[Ww][Ii][Nn][Dd][Oo][Ww][Ss]/[Ss][Yy][Ss][Tt][Ee][Mm]32/.*"
			".*[Ww][Ii][Nn][Dd][Oo][Ww][Ss]/[Ss][Yy][Ss][Ww][Oo][Ww]64/.*"

			# Windows WinSxS directory.
			".*[Ww][Ii][Nn][Dd][Oo][Ww][Ss]/[Ww][Ii][Nn][Ss][Xx][Ss]/.*"

			# Windows API-set DLLs.
			"api-ms-.*"
			"ext-ms-.*"

			# macOS system libraries and frameworks.
			".*[/\\\\]System[/\\\\]Library[/\\\\].*"

			# Linux core system libraries.
			".*/libc\\.so.*"
			".*/libm\\.so.*"
			".*/libdl\\.so.*"
			".*/libpthread\\.so.*"
			".*/librt\\.so.*"
			".*/ld-linux.*"

		RUNTIME DESTINATION "${DEPLOY_BINDIR}"
		LIBRARY DESTINATION "${DEPLOY_LIBDIR}"
		FRAMEWORK DESTINATION "${DEPLOY_LIBDIR}"
	)

endfunction()

function(blackthorn_install_assets DIR)
	install(
		DIRECTORY "${DIR}/"
		DESTINATION "${DEPLOY_BINDIR}/assets"
	)
endfunction()