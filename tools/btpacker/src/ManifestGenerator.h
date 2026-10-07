#pragma once

#include <filesystem>
#include <ostream>
#include <string>
#include <vector>

#include "Manifest.h"

namespace BTPacker {

/**
 * @class ManifestGenerator
 * @brief Scans an asset directory and generates a .pack.json manifest.
 *
 * Re-running gen-manifest over the same directory regenerates the
 * file, so it is safe to use as part of a build step as long as
 * manual edits are tracked separately (e.g. by committing the
 * generated manifest to version control and editing it there).
 *
 * @section usage Usage
 * @code
 * btpacker generate-manifest
 *     --assets   assets/
 *     --output   assets.btp
 *     --manifest manifest.json
 *     [--exclude video]
 *     [--exclude fonts]
 *     [--level 3]
 *     [--no-symbols]
 * @endcode
 */
class ManifestGenerator {
public:
	struct Options {
		std::filesystem::path assetDir;       ///< Root directory to scan (required).
		std::filesystem::path manifestOut;    ///< Path to write the generated .pack.json (required).
		std::filesystem::path btpOutput;      ///< Value of "output" in the manifest (required).
		int                   compressionLevel = 3;
		bool                  writeSymbolTable = true;
		std::vector<std::string> excludeDirs;  ///< Subdirectory names to skip entirely (e.g. "video").
	};

	/**
	 * @brief Scans @p opts.assetDir and writes a manifest to @p opts.manifestOut.
	 *
	 * @param opts  Generation options.
	 * @param log   Stream for progress output.
	 * @return true on success.
	 */
	static bool generate(const Options& opts, std::ostream& log);

private:
	/// Maps a file extension (lowercase, with dot) to a PackAssetType name string.
	static std::string classifyExtension(const std::string& ext);

	/**
	 * @brief Writes the collected asset list as a formatted JSON manifest.
	 *
	 * @param opts   Generation options (output path, btp output, level, symbols).
	 * @param assets Collected asset entries.
	 * @param log    Stream for progress output.
	 * @return true on success.
	 */
	static bool writeManifest(
		const Options&                    opts,
		const std::vector<ManifestAsset>& assets,
		std::ostream&                     log,
		std::filesystem::path             metadataPath
	);
};

} // namespace BTPacker
