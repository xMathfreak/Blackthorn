#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Core/Export.h"

namespace Blackthorn::Assets {

/// Four-byte magic identifier: "BTP\0" (Blackthorn Pack).
constexpr uint32_t BTP_MAGIC = 0x00505442u;
constexpr uint32_t BTP_VERSION = 2u;

/**
 * @brief Compression codec tag stored per BTPEntry.
 *
 * Stored as a uint8_t in the binary format. Additional codecs can be added
 * in future versions without breaking the format. Loaders check this field
 * before decompressing.
 */
enum class PackCompression : uint8_t {
	None = 0, ///< Raw, uncompressed bytes.
	Zstd = 1, ///< zstd frame (default).
};

/**
 * @brief Broad asset category stored per BTPEntry.
 *
 * Used by the AssetResolver to select the correct IAssetLoader at runtime
 * without needing to inspect file extension strings.
 */
enum class PackAssetType : uint8_t {
	Unknown = 0,
	Texture = 1,
	Audio = 2,
	Shader = 3,
	Font = 4,
	SpriteClip = 5,
	ParticleEffect = 6,
	Localization = 7,
	Raw = 255,
};

/**
 * @brief Fixed 64-byte file header. Always located at byte offset 0.
 *
 * Written last by the packer (after all blobs and the TOC are on disk) by
 * seeking back to offset 0. The reader seeks to tocOffset to load the TOC
 * without scanning the file.
 *
 * Layout (all fields little-endian):
 * @code
 *  0   magic            uint32   "BTP\0"
 *  4   version          uint32   BTP_VERSION
 *  8   flags            uint32   Reserved, must be 0
 * 12   entryCount       uint32   Number of TOC entries
 * 16   tocOffset        uint64   Byte offset of compressed TOC block
 * 24   tocCompSize      uint64   Byte size of compressed TOC block
 * 32   tocUncompSize    uint64   Byte size of TOC after decompression
 * 40   symbolTableOff   uint64   Byte offset of symbol table (0 = absent)
 * 48   symbolTableSize  uint64   Byte size of symbol table (0 = absent)
 * 56   metadataOff      uint64   Byte offset of pack metadata JSON (0 = absent)
 * 64   metadataSize     uint64   Byte size of pack metadata JSON (0 = absent)
 * @endcode
 */
#pragma pack(push, 1)
struct BTPHeader {
	uint32_t magic; ///< Must equal BTP_MAGIC.
	uint32_t version; ///< Must equal BTP_VERSION.
	uint32_t flags; ///< Reserved for future use, must be 0.
	uint32_t entryCount; ///< Number of entries in the TOC.
	uint64_t tocOffset; ///< Byte offset from file start to the compressed TOC block.
	uint64_t tocCompSize; ///< Byte size of the compressed TOC block on disk.
	uint64_t tocUncompSize; ///< Byte size of the TOC block after decompression.
	uint64_t symbolTableOff; ///< Byte offset of the debug symbol table. 0 = not present.
	uint64_t symbolTableSize; ///< Byte size of the debug symbol table. 0 = not present.
	uint64_t metadataOff; ///< Byte offset of the pack metadata JSON block. 0 = not present.
	uint64_t metadataSize; ///< Byte size of the pack metadata JSON block. 0 = not present.
};
static_assert(sizeof(BTPHeader) == 72, "BTPHeader must be exactly 72 bytes");

/**
 * @brief One entry in the table of contents (TOC).
 *
 * The TOC is stored as a flat array of BTPEntry structs, compressed as a
 * single zstd block and written after all asset data blobs. Individual
 * entries are randomly accessible after the TOC is decompressed into memory
 * at mount time.
 *
 * Layout (all fields little-endian):
 * @code
 *  0   assetID          uint64   xxHash64 of the asset string ID
 *  8   dataOffset       uint64   Byte offset of the compressed blob from file start
 * 16   compressedSize   uint64   Byte size of the compressed blob on disk
 * 24   uncompressedSize uint64   Byte size after decompression (used to pre-allocate)
 * 32   xxhash           uint64   xxHash64 of the compressed blob (integrity check)
 * 40   assetType        uint8    PackAssetType enum value
 * 41   compression      uint8    PackCompression enum value
 * 42   padding          uint8[6]
 * @endcode
 */
struct BTPEntry {
	uint64_t assetID; ///< xxHash64 of the asset string ID.
	uint64_t dataOffset; ///< Byte offset of the compressed data blob from file start.
	uint64_t compressedSize; ///< Compressed byte size on disk.
	uint64_t uncompressedSize; ///< Uncompressed byte size, pre-allocate buffers to this.
	uint64_t xxhash; ///< xxHash64 of the compressed blob for integrity verification.
	PackAssetType assetType; ///< Broad asset category.
	PackCompression compression; ///< Codec used to compress this entry.
	uint8_t padding[6]; ///< Explicit padding to a 48-byte stride.
};
static_assert(sizeof(BTPEntry) == 48, "BTPEntry must be exactly 48 bytes");
#pragma pack(pop)


/**
 * @brief One entry of a pack's "dependencies" list: another pack that must be
 * present, and the version range of it that is acceptable.
 */
struct PackDependency {
	/// Stable ID of the required pack (see PackMetadata::id).
	std::string id;

	/// npm-style semver range (see SemVerRange). Empty means any version.
	std::string versionRange;

	bool operator==(const PackDependency& other) const noexcept {
		return id == other.id && versionRange == other.versionRange;
	}

	bool operator<(const PackDependency& other) const noexcept {
		if (id != other.id)
			return id < other.id;

		return versionRange < other.versionRange;
	}
};

/**
 * @brief Optional, free-form metadata describing a pack as a whole (its
 * name, author, version, etc.), as opposed to any single asset within it.
 *
 * Populated by the packer from an optional "*.metadata" JSON file found at
 * the root of the manifest's asset directory (e.g. "assets/mymod.metadata").
 * Every field is independently optional; an empty string (or empty list) means
 * the field was absent from the source file (or the source file itself was
 * absent).
 *
 * Stored as a small, uncompressed JSON blob in the pack itself (see
 * BTPHeader::metadataOff / metadataSize), so it can be read back by
 * PackMount at mount time, by btpacker's list command, or by any other
 * tool without decompressing or even reading the TOC.
 *
 * @par Identity and ordering
 * @c id is the pack's stable identity (reverse-domain style, e.g.
 * "com.example.mod") and is what dependencies, ordering hints and conflicts
 * refer to. @c name is display text only. @c version should be valid SemVer
 * 2.0.0 for the pack to satisfy anyone's dependency range. A pack without an
 * @c id can still be mounted, but cannot take part in load-order resolution
 * or content manifests.
 *
 * @par Source file format
 * @code{.json}
 * {
 *     "id": "com.example.mod",
 *     "name": "modname",
 *     "shortDescription": "Short description of the mod",
 *     "longDescription": "A more elaborate description of the functions of the mod",
 *     "author": "Mod Creator",
 *     "version": "3.2.1",
 *     "dependencies": [ { "id": "com.other.mod", "version": "^1.2.3" } ],
 *     "loadBefore": [ "com.some.mod" ],
 *     "loadAfter": [ "com.other.mod" ],
 *     "conflicts": [ "com.conflicting.mod" ]
 * }
 * @endcode
 *
 * The list fields are sets: the packer sorts and de-duplicates them so the
 * embedded block (and therefore the pack digest) does not depend on the order
 * they were written in.
 */
struct BLACKTHORN_API PackMetadata {
	std::string id;
	std::string name;
	std::string shortDescription;
	std::string longDescription;
	std::string author;
	std::string version;

	/// Packs that must be present (and in an acceptable version range).
	/// Each dependency is also implicitly loaded before this pack.
	std::vector<PackDependency> dependencies;

	/// Pack IDs that, if present, should be loaded after this pack.
	std::vector<std::string> loadBefore;

	/// Pack IDs that, if present, should be loaded before this pack.
	std::vector<std::string> loadAfter;

	/// Pack IDs that cannot be present at the same time as this pack.
	std::vector<std::string> conflicts;

	/** @brief Returns true if every field is empty (no metadata was found/embedded). */
	bool empty() const noexcept {
		return id.empty()
			&& name.empty()
			&& shortDescription.empty()
			&& longDescription.empty()
			&& author.empty()
			&& version.empty()
			&& dependencies.empty()
			&& loadBefore.empty()
			&& loadAfter.empty()
			&& conflicts.empty();
	}
};

} // namespace Blackthorn::Assets
