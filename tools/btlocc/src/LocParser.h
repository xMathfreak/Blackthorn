#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "LocManifest.h"

namespace BTLocC {

/**
 * @class LocParser
 * @brief Parses a locale JSON file (the same schema LocalizationManager's
 *        parseLocaleFile reads at runtime) into a LocFile.
 *
 * Error handling: all errors are printed to stderr (as "path:line: error:
 * ..."); the function returns std::nullopt on any error. The caller treats
 * nullopt as fatal.
 */
class LocParser {
public:
	static std::optional<LocFile> parse(const std::filesystem::path& path);

private:
	explicit LocParser(std::string src, std::string pathForErrors)
		: source(std::move(src))
		, path(std::move(pathForErrors))
	{}

	/// Entry point: parses source into result.
	bool run();

	/// Skips whitespace and "//" line comments (not standard JSON, but
	/// convenient, matching ManifestParser's own leniency).
	void skipWS();

	/// Returns the current character without consuming it, or '\0' at EOF.
	char peek() const;

	/// Consumes and returns the current character, tracking line number.
	char consume();

	/// Skips whitespace, then expects and consumes ch. Errors if not found.
	bool expect(char ch);

	/// Parses a JSON string value, including escape sequences and \\uXXXX
	/// (encoded to UTF-8; BMP code points only with no surrogate pairs).
	bool parseString(std::string& out);

	/// Parses the top-level {"localeCode": ..., "entries": {...}} object.
	bool parseTopLevel();

	/// Parses the "entries" object into result.entries.
	bool parseEntriesObject();

	/// Parses one entry value: a plain string, or an object with "text"/
	/// plural-category keys and an optional "context".
	bool parseEntryValue(LocEntryValue& out);

	std::string source;
	std::string path;
	size_t position = 0;
	int line = 1;
	LocFile result;

	/// Emits "path:line: error: msg" to stderr.
	void error(const std::string& msg) const;
};

} // namespace BTLocC
