#pragma once

#include "Core/Types/Numeric.h"

namespace Blackthorn::Assets {

constexpr U32 BTP_MAGIC = 0x00505442u; // "BTP\0"
constexpr U32 BTP_VERSION = 1u;

/// @brief Compression codec stored per-entry.
enum class PackCompression : U8 {
	None = 0,
	Zstd = 1
};

/// @brief Broad category used by the resolver to pick the right @c IAssetLoader.
enum class PackAssetType : U8 {
	Unknown = 0,
	Texture = 1,
	Audio = 2,
	Shader = 3,
	Font = 4,
	Raw = U8_MAX
};

/// @brief Fixed 64-byte file header. Always at byte 0.
struct BTPHeader {
	U32 magic; ///< Must equal BTP_MAGIC.
	U32 version; ///< Must equal BTP_VERSION.
	U32 flags; ///< Reserved, must be 0.
	U32 entryCount; ///< Number of TOC entries.
	U32 tocOffset; ///< Byte offset of compressed TOC block.
	U64 tocCompSize; ///< Byte offset of compressed TOC block.
	U64 tocUncompSize; ///< Byte size of uncompressed TOC block.
	U64 symbolTableOff; ///< 0 if no symbol table.
	U64 symbolTableSize; ///< 0 if no symbol table.
	U8 reserved[8];
};

static_assert(sizeof(BTPHeader) == 64, "BTPHeader must be exactly 64 bytes");

/// @brief One entry in the uncompressed TOC. Stored as a flat array.
struct BTPEntry {
	U64 assetID; ///< xxHash64 of the asset string ID.
	U64 dataOffset; ///< Byte offset from file start.
	U64 compressedSize; ///< Byte size of compressed blob.
	U64 uncompressedSize; ///< Byte size after decompression.
	U64 xxhash; ///< xxHash64 of the compressed blob.
	PackAssetType assetType;
	PackCompression compression;
	U8 padding[6];
};

static_assert(sizeof(BTPEntry) == 48, "BTPEntry must be exactly 48 bytes");

} // namespace Blackthorn::Assets