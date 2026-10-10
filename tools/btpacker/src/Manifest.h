#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace BTPacker {

/**
 * @brief One asset entry inside a pack manifest.
 */
struct ManifestAsset {
	std::filesystem::path sourcePath; ///< Path relative to the input directory. Required.
	std::string typeStr;              ///< One of the type names listed on PackManifest. Required.
};

/**
 * @brief Parsed representation of a .pack.json manifest.
 *
 * A manifest lists the assets of one project, relative to that project's input
 * directory. It doesn't say where the pack will be written. The pack step
 * supplies the input directory and output path.
 *
 * @code{.json}
 * {
 *     "root": "assets",
 *     "compression_level": 3,
 *     "symbol_table": true,
 *     "metadata": "assets.metadata",
 *     "assets": [
 *         { "path": "textures/player.png", "type": "Texture" },
 *         { "path": "audio/bgm.ogg",       "type": "Audio"   }
 *     ]
 * }
 * @endcode
 *
 * Fields:
 *   root              - virtual root name, prefixed to every asset ID. Defaults to the input
 *                       directory's name. Must be a single name: no '/', '\', ':', or '@', not
 *                       "." or "..", and not "root" (reserved for references).
 *   compression_level - zstd level 1-22; default 3
 *   metadata          - metadata file, relative to the input directory (optional)
 *   symbol_table      - write a debug symbol table; default true
 *   assets            - non-empty array of asset objects (required)
 *
 * Each asset object:
 *   path - path relative to the input directory. The asset ID is root + "/" + path,
 *          for example "assets/textures/player.png". (required)
 *   type - "Texture" | "Audio" | "Shader" | "Font" | "SpriteClip" | "ParticleEffect" |
 *          "Localization" | "Raw" (required)
 */
struct PackManifest {
	std::string virtualRoot;          ///< Prefix for every asset ID. Validated by isValidRootName().
	std::filesystem::path metadataPath; ///< Relative to the input directory. Empty if none.
	int compressionLevel = 3;
	bool writeSymbolTable = true;
	std::vector<ManifestAsset> assets;
};

} // namespace BTPacker
