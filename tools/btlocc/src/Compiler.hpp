#pragma once

#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace BTLocC {

struct CompileOptions {
	/// All locale JSON files to compile, in the order given on the command
	/// line. The first one is the base/canonical locale unless basePath
	/// names a different one.
	std::vector<std::filesystem::path> files;

	/// If set, must equal one entry in files. That entry is treated as the
	/// base/canonical locale instead of files.front().
	std::optional<std::filesystem::path> basePath;

	std::filesystem::path outDir = ".";
	int compressionLevel = 3;

	/// Escalate base/translation key-set mismatches from warnings to a
	/// hard failure. Off by default: missing translations are the normal
	/// state of an in-progress localization pass, not a build breaker.
	/// Meant for a CI job that wants complete-translation enforcement.
	bool strict = false;

	/// Embed a debug symbol table (TextID -> original key) in each .btloc
	/// file, mirroring btpacker's --no-symbols. On by default. This is
	/// what lets runtime diagnostics name an entry instead of printing a
	/// bare hash.
	bool writeSymbolTable = true;

	/// If set, also generate a C++ header of TextID constants, one per key
	/// in the base locale (translations don't affect the header; keys are
	/// canonical, text isn't).
	std::optional<std::filesystem::path> headerPath;
	std::string headerNamespace = "Loc";
};

/**
 * @class Compiler
 * @brief Drives the whole btlocc pipeline: parse base + translations,
 *        cross-validate their key sets, detect duplicate keys / TextID
 *        collisions, then emit one .btloc per locale and (optionally) the
 *        generated TextID header.
 */
class Compiler {
public:
	/// @return true on success. All diagnostics go to stderr; a short
	///         per-file summary is written to log on success.
	static bool compile(const CompileOptions& opts, std::ostream& log);
};

} // namespace BTLocC
