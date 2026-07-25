#[[
	PlatformResources.cmake

	Central metadata + packaging system for Blackthorn executables.

	Any executable target (game, editor, tool) declares what it IS via
	target_set_metadata(), then target_add_platform_resources() decides
	how that identity is represented on the current platform:

	  - Windows	: generates a .rc file (icon + VERSIONINFO) and adds it
				  to the target's sources.
	  - macOS	: generates an Info.plist and wires it up via
				  MACOSX_BUNDLE_INFO_PLIST (only meaningful if the target
				  also has MACOSX_BUNDLE TRUE).
	  - Linux	: generates a .desktop file next to the build output.

	On every platform it also generates a small C++ header
	(Generated/BlackthornMetadata.h, unique per target) exposing
	Blackthorn::Generated::getMetadataConfig(), so the same values that
	produced the OS resources can be handed to MetadataConfig at runtime.

	Usage:

	  target_set_metadata(App
		  NAME       "App Name"
		  VERSION    "${PROJECT_VERSION}"
		  IDENTIFIER "com.app.identifier"
		  COMPANY    "App Studio"
		  AUTHOR     "John Doe"
		  COPYRIGHT  "Copyright (C) 2077 App Studio"
		  URL        "https://example.com"
		  TYPE       "game"
		  ICON       "${CMAKE_SOURCE_DIR}/assets/icons/app.ico"
	  )
	  target_add_platform_resources(App)
]]

# Directory containing the .in templates (windows/, macos/, linux/, and the
# shared C++ header template). Set once here so callers never need to know
# where the templates live.
set(BLACKTHORN_RESOURCES_DIR "${CMAKE_CURRENT_LIST_DIR}/../resources" CACHE INTERNAL
	"Directory containing platform resource templates"
)

# Helper: fetch a BT_META_* target property into a local variable, treating
# an unset property (CMake's "<...>-NOTFOUND") the same as an empty string,
# so downstream configure_file() calls never emit the literal word NOTFOUND.
function(_bt_get_meta_prop out_var target prop_name)
	get_target_property(_value "${target}" "${prop_name}")

	if("${_value}" MATCHES "-NOTFOUND$")
		set(_value "")
	endif()

	set(${out_var} "${_value}" PARENT_SCOPE)
endfunction()

# target_set_metadata(<target>
#	[NAME <name>] [VERSION <x.y.z>] [IDENTIFIER <reverse-dns>]
#	[COMPANY <name>] [AUTHOR <name>] [COPYRIGHT <text>] [URL <url>]
#	[TYPE <game|application|mediaplayer|...>] [ICON <path>]
# )
#
# Records a target's identity as target properties (BT_META_*). This is the
# single source of truth later read by target_add_platform_resources().
# All arguments are optional and fall back to sensible defaults so this can
# be called with just the fields that matter for a given target (e.g. a
# tool might only set NAME and TYPE).
function(target_set_metadata target)
	set(oneValueArgs NAME VERSION IDENTIFIER COMPANY AUTHOR COPYRIGHT URL TYPE ICON)
	cmake_parse_arguments(META "" "${oneValueArgs}" "" ${ARGN})

	if(NOT TARGET ${target})
		message(FATAL_ERROR "target_set_metadata(): '${target}' is not a target")
	endif()

	if(NOT META_NAME)
		set(META_NAME "${target}")
	endif()

	if(NOT META_VERSION)
		if(PROJECT_VERSION)
			set(META_VERSION "${PROJECT_VERSION}")
		else()
			set(META_VERSION "0.0.0")
		endif()
	endif()

	if(NOT META_IDENTIFIER)
		string(TOLOWER "com.blackthorn.${target}" META_IDENTIFIER)
	endif()

	if(NOT META_TYPE)
		set(META_TYPE "application")
	endif()

	string(REPLACE "." ";" _version_list "${META_VERSION}")
	list(LENGTH _version_list _version_len)

	if(_version_len GREATER 0)
		list(GET _version_list 0 _version_major)
	else()
		set(_version_major 0)
	endif()

	if(_version_len GREATER 1)
		list(GET _version_list 1 _version_minor)
	else()
		set(_version_minor 0)
	endif()

	if(_version_len GREATER 2)
		list(GET _version_list 2 _version_patch)
	else()
		set(_version_patch 0)
	endif()

	if(META_ICON AND NOT EXISTS "${META_ICON}")
		message(WARNING "target_set_metadata(${target}): ICON '${META_ICON}' does not exist, ignoring")
		set(META_ICON "")
	endif()

	set_target_properties(${target} PROPERTIES
		BT_META_NAME           "${META_NAME}"
		BT_META_VERSION        "${META_VERSION}"
		BT_META_VERSION_MAJOR  "${_version_major}"
		BT_META_VERSION_MINOR  "${_version_minor}"
		BT_META_VERSION_PATCH  "${_version_patch}"
		BT_META_IDENTIFIER     "${META_IDENTIFIER}"
		BT_META_COMPANY        "${META_COMPANY}"
		BT_META_AUTHOR         "${META_AUTHOR}"
		BT_META_COPYRIGHT      "${META_COPYRIGHT}"
		BT_META_URL            "${META_URL}"
		BT_META_TYPE           "${META_TYPE}"
		BT_META_ICON           "${META_ICON}"
	)
endfunction()

# target_add_platform_resources(<target>)
#
# Reads the BT_META_* properties set by target_set_metadata() and generates
# the platform-appropriate resource(s) for <target>, plus the shared
# Generated/BlackthornMetadata.h header. Must be called after
# target_set_metadata(target ...) for the same target.
function(target_add_platform_resources target)
	if(NOT TARGET ${target})
		message(FATAL_ERROR "target_add_platform_resources(): '${target}' is not a target")
	endif()

	get_target_property(NAME "${target}" BT_META_NAME)
	if(NOT NAME)
		message(FATAL_ERROR
			"target_add_platform_resources(${target}): no metadata set. "
			"Call target_set_metadata(${target} ...) first."
		)
	endif()

	_bt_get_meta_prop(VERSION        ${target} BT_META_VERSION)
	_bt_get_meta_prop(VERSION_MAJOR  ${target} BT_META_VERSION_MAJOR)
	_bt_get_meta_prop(VERSION_MINOR  ${target} BT_META_VERSION_MINOR)
	_bt_get_meta_prop(VERSION_PATCH  ${target} BT_META_VERSION_PATCH)
	_bt_get_meta_prop(IDENTIFIER     ${target} BT_META_IDENTIFIER)
	_bt_get_meta_prop(COMPANY        ${target} BT_META_COMPANY)
	_bt_get_meta_prop(AUTHOR         ${target} BT_META_AUTHOR)
	_bt_get_meta_prop(COPYRIGHT      ${target} BT_META_COPYRIGHT)
	_bt_get_meta_prop(URL            ${target} BT_META_URL)
	_bt_get_meta_prop(TYPE           ${target} BT_META_TYPE)
	_bt_get_meta_prop(ICON           ${target} BT_META_ICON)

	set(TARGET_NAME "${target}")
	set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/${target}")

	configure_file(
		"${BLACKTHORN_RESOURCES_DIR}/BlackthornMetadata.h.in"
		"${GEN_DIR}/include/Generated/BlackthornMetadata.h"
		@ONLY
	)
	target_include_directories(${target} PRIVATE "${GEN_DIR}/include")

	if(WIN32)
		if(ICON)
			file(TO_CMAKE_PATH "${ICON}" ICON_FORWARD_SLASH)
			set(ICON_STATEMENT "IDI_ICON ICON \"${ICON_FORWARD_SLASH}\"")
		else()
			set(ICON_STATEMENT "// No icon specified (ICON not passed to target_set_metadata)")
		endif()

		configure_file(
			"${BLACKTHORN_RESOURCES_DIR}/windows/app.rc.in"
			"${GEN_DIR}/app.rc"
			@ONLY
		)
		target_sources(${target} PRIVATE "${GEN_DIR}/app.rc")

	elseif(APPLE)
		if(ICON)
			get_filename_component(ICON_FILENAME "${ICON}" NAME)
			set(ICON_PLIST_ENTRY "\t<key>CFBundleIconFile</key>\n\t<string>${ICON_FILENAME}</string>")
		else()
			set(ICON_PLIST_ENTRY "\t<!-- No icon specified (ICON not passed to target_set_metadata) -->")
		endif()

		configure_file(
			"${BLACKTHORN_RESOURCES_DIR}/macos/Info.plist.in"
			"${GEN_DIR}/Info.plist"
			@ONLY
		)
		set_target_properties(${target} PROPERTIES
			MACOSX_BUNDLE_INFO_PLIST "${GEN_DIR}/Info.plist"
		)

		if(ICON)
			set_target_properties(${target} PROPERTIES
				MACOSX_BUNDLE_ICON_FILE "${ICON_FILENAME}"
			)
			target_sources(${target} PRIVATE "${ICON}")
			set_source_files_properties("${ICON}" PROPERTIES
				MACOSX_PACKAGE_LOCATION "Resources"
			)
		endif()

	elseif(UNIX)
		if(ICON)
			get_filename_component(ICON_NAME "${ICON}" NAME_WE)
		else()
			set(ICON_NAME "")
		endif()

		if(TYPE STREQUAL "game")
			set(LINUX_CATEGORY "Game")
		else()
			set(LINUX_CATEGORY "Utility")
		endif()

		configure_file(
			"${BLACKTHORN_RESOURCES_DIR}/linux/app.desktop.in"
			"${GEN_DIR}/${target}.desktop"
			@ONLY
		)
	else()
		message(STATUS "target_add_platform_resources(${target}): no resource generation for this platform")
	endif()
endfunction()
