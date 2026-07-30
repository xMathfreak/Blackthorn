#pragma once

#include <cstdint>

namespace Blackthorn::Assets {

/// Four-byte magic identifier: "BTP\0" (Blackthorn Pack).
constexpr uint32_t BTP_MAGIC = 0x00505442u;
constexpr uint32_t BTP_VERSION = 1u;

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
 * 56   reserved         uint8[8]
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
	uint8_t reserved[8];
};
static_assert(sizeof(BTPHeader) == 64, "BTPHeader must be exactly 64 bytes");

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

} // namespace Blackthorn::Assets
