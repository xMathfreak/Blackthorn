#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace BTPacker {

/**
 * @brief Describes a single asset entry inside a pack manifest.
 */
struct ManifestAsset {
	std::string id; ///< String asset ID (hashed at pack time).
	std::filesystem::path sourcePath; ///< Absolute path to the source file on disk.
	std::string typeStr; ///< "Texture" | "Audio" | "Shader" | "Font" | "SpriteClip" | "Raw"
};

/**
 * @brief Parsed representation of a .pack.json manifest file.
 *
 * Example manifest:
 * @code{.json}
 * {
 *     "output": "data/base.btp",
 *     "compression_level": 3,
 *     "symbol_table": true,
 *     "assets": [
 *         { "id": "player_tex",  "path": "assets/textures/player.png", "type": "Texture" },
 *         { "id": "bg_music",    "path": "assets/audio/bgm.ogg",       "type": "Audio"   },
 *         { "id": "main_shader", "path": "assets/shaders/main.glsl",   "type": "Shader"  }
 *     ]
 * }
 * @endcode
 *
 * Fields:
 *   output            - path to the .btp file to create (required)
 *   compression_level - zstd level 1–22; default 3
 *   symbol_table      - write a debug symbol table; default true
 *   assets            - array of asset objects (required, must be non-empty)
 *
 * Each asset object:
 *   id   - string ID used at runtime to look up the asset (required)
 *   path - source file path, relative to the manifest file or absolute (required)
 *   type - "Texture" | "Audio" | "Shader" | "Font" | "SpriteClip" | "Raw" (required)
 */
struct PackManifest {
	std::filesystem::path outputPath;
	std::filesystem::path manifestDir; ///< Directory of the manifest file; asset paths are resolved relative to this.
	int compressionLevel = 3;
	bool writeSymbolTable = true;
	std::vector<ManifestAsset> assets;
};

} // namespace BTPacker