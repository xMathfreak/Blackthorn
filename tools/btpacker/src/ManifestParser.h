#pragma once

#include <filesystem>
#include <optional>

#include "Manifest.h"

namespace BTPacker {

/**
 * @class ManifestParser
 * @brief Parses a .pack.json manifest file into a PackManifest struct.
 *
 * The parser is a minimal hand-rolled JSON reader that handles exactly the
 * subset of JSON used by pack manifests: a top-level object, string/integer/
 * boolean scalar values, and a single array of objects. It does not attempt
 * to be a general-purpose JSON library.
 *
 * Error handling: all errors are printed to stderr and the function returns
 * std::nullopt. The caller (main.cpp) treats nullopt as a fatal parse error
 * and exits with a non-zero status.
 */
class ManifestParser {
public:
	/**
	 * @brief Parses the manifest file at @p path.
	 *
	 * Asset paths listed in the manifest are resolved relative to the
	 * directory containing the manifest file so manifests are portable.
	 *
	 * @param path Path to the .pack.json file.
	 * @return Populated PackManifest on success; std::nullopt on any error.
	 */
	static std::optional<PackManifest> parse(const std::filesystem::path& path);

private:
	explicit ManifestParser(std::string src, std::filesystem::path dir)
		: source(std::move(src))
		, manifestDir(std::move(dir))
	{}

	/// Entry point: parses source and fills manifest.
	bool run();

	/// Skips whitespace and JSON comments (not standard, but convenient).
	void skipWS();

	/// Consumes the next character. Returns '\0' at end of input.
	char peek() const;

	/// Advances past the current character and returns it.
	char consume();

	/// Expects and consumes @p ch. Returns false and emits an error if not found.
	bool expect(char ch);

	/// Parses a JSON string value (including surrounding quotes).
	bool parseString(std::string& out);

	/// Parses a JSON integer value.
	bool parseInt(int& out);

	/// Parses a JSON boolean value (true | false).
	bool parseBool(bool& out);

	/// Parses the top-level JSON object.
	bool parseTopLevel();

	/// Parses the "assets" JSON array.
	bool parseAssetsArray();

	/// Parses one asset object inside the array.
	bool parseAssetObject(ManifestAsset& out);

	std::string source;
	std::filesystem::path manifestDir;
	size_t position = 0;
	int line = 1;
	PackManifest manifest;

	/// Emits "file:line: error: <msg>" to stderr.
	void error(const std::string& msg) const;
};

} // namespace BTPacker