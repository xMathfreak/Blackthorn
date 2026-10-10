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
 * Every asset path in the generated manifest is relative to @c projectRoot,
 * which is also the engine's runtime working directory. The path string is
 * the asset ID, so a path that loads in loose-file mode is exactly the path
 * that packs under the same ID.
 *
 * @section usage Usage
 * @code
 * btpacker generate-manifest
 *     [--root my_mod]
 *     [--exclude video]
 *     [--level 3]
 *     [--no-symbols]
 *     <input>
 *     <output>
 * @endcode
 */
class ManifestGenerator {
public:
	struct Options {
		std::filesystem::path input;        ///< Path to scan for assets.
		std::filesystem::path output;       ///< Path to write the generated .pack.json.
		std::string root;                   ///< Virtual root path written into the packer.
		int                   compressionLevel = 3;
		bool                  writeSymbolTable = true;
		std::vector<std::string> excludeDirs;    ///< Subdirectory names to skip entirely (e.g. "video").
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
	 * @param opts         Generation options.
	 * @param assets       Collected asset entries, paths relative to projectRoot.
	 * @param log          Stream for progress output.
	 * @param metadataPath Metadata path relative to projectRoot, or empty.
	 * @return true on success.
	 */
	static bool writeManifest(
		const Options&                    opts,
		const std::vector<ManifestAsset>& assets,
		std::ostream&                     log,
		const std::string&                metadataPath
	);
};

} // namespace BTPacker
