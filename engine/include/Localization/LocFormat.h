#pragma once

#include <cstdint>

namespace Blackthorn::Localization {

/// Four-byte magic identifier: "BTL\0" (Blackthorn Localization).
constexpr uint32_t BTLOC_MAGIC = 0x004C5442u;
constexpr uint32_t BTLOC_VERSION = 1u;

/// Sentinel for BTLOCEntry::formOffset[i] / contextOffset meaning "this slot
/// isn't present". A real offset (including 0) plus a length of 0 means the
/// slot IS present, holding an empty string. Absence and emptiness are
/// deliberately distinct, so e.g. { "values.empty_string": "" } round-trips
/// correctly instead of being mistaken for an unset category.
constexpr uint64_t BTLOC_ABSENT_OFFSET = UINT64_MAX;

/**
 * @brief Fixed 64-byte file header. Always located at byte offset 0.
 *
 * Written last by the compiler (after the compressed block is on disk) by
 * seeking back to offset 0, mirroring .btp's BTPHeader.
 *
 * Layout (all fields little-endian):
 * @code
 *  0   magic            uint32    "BTL\0"
 *  4   version          uint32    BTLOC_VERSION
 *  8   flags            uint32    Reserved, must be 0
 * 12   entryCount       uint32    Number of BTLOCEntry records in the block
 * 16   localeCode       char[16]  Null-terminated/padded, e.g. "en-US"
 * 32   compSize         uint64    Byte size of the compressed block on disk
 * 40   uncompSize       uint64    Byte size of [entries][string blob]
 *                                  combined, after decompression
 * 48   symbolTableOff   uint64    Byte offset of the debug symbol table
 *                                  (0 = absent)
 * 56   symbolTableSize  uint64    Byte size of the debug symbol table
 *                                  (0 = absent)
 * @endcode
 */
#pragma pack(push, 1)
struct BTLOCHeader {
	uint32_t magic; ///< Must equal BTLOC_MAGIC.
	uint32_t version; ///< Must equal BTLOC_VERSION.
	uint32_t flags; ///< Reserved for future use, must be 0.
	uint32_t entryCount; ///< Number of BTLOCEntry records.
	char localeCode[16]; ///< Null-terminated locale code, e.g. "en-US".
	uint64_t compSize; ///< Compressed size of the combined entries+strings block.
	uint64_t uncompSize; ///< Decompressed size of that block.
	uint64_t symbolTableOff; ///< Byte offset of the debug symbol table. 0 = not present.
	uint64_t symbolTableSize; ///< Byte size of the debug symbol table. 0 = not present.
};
static_assert(sizeof(BTLOCHeader) == 64, "BTLOCHeader must be exactly 64 bytes");

/**
 * @brief One entry's plural-category slots, indexed by PluralCategory's
 *        declared order (0 = Zero, 1 = One, 2 = Two, 3 = Few, 4 = Many,
 *        5 = Other). Category indexes directly so no lookup table is needed.
 *
 * @details There's no separate "is this entry plural?" flag: a plain
 *          (non-pluralized) entry is simply one whose only present slot is
 *          Other, since Other is already the universal fallback category
 *          LocalizationManager::resolveTemplate() falls back to.
 *
 *          formOffset[i] == BTLOC_ABSENT_OFFSET means that category isn't
 *          present (formLength[i] is meaningless in that case). Any other
 *          offset value (including 0) means the category is present,
 *          with formLength[i] giving its byte length (which may legitimately
 *          be 0, for an empty-string form). Offsets are relative to the
 *          start of the string blob (i.e. relative to byte
 *          entryCount * sizeof(BTLOCEntry) within the decompressed block),
 *          not to the start of the file or the block.
 *
 *          contextOffset/contextLength follow the same present-vs-absent
 *          convention, independently of the form slots.
 *
 * Layout (all fields little-endian):
 * @code
 *  0   textID          uint64      XXH64(seed 0) of the source JSON key
 *  8   formOffset[6]   uint64[6]   Per-PluralCategory offset into the string
 *                                   blob; BTLOC_ABSENT_OFFSET = not present
 * 56   formLength[6]   uint32[6]   Per-PluralCategory byte length
 * 80   contextOffset   uint64      Offset of the context string;
 *                                   BTLOC_ABSENT_OFFSET = no context
 * 88   contextLength   uint32      Byte length
 * 92   padding         uint8[4]
 * @endcode
 */
struct BTLOCEntry {
	uint64_t textID; ///< XXH64(seed 0) of the source JSON key.
	uint64_t formOffset[6]; ///< Per-PluralCategory string blob offset; BTLOC_ABSENT_OFFSET = not present.
	uint32_t formLength[6]; ///< Per-PluralCategory byte length (valid only when formOffset[i] != BTLOC_ABSENT_OFFSET).
	uint64_t contextOffset; ///< String blob offset of the context string; BTLOC_ABSENT_OFFSET = no context.
	uint32_t contextLength; ///< Byte length (valid only when contextOffset != BTLOC_ABSENT_OFFSET).
	uint8_t padding[4];
};
static_assert(sizeof(BTLOCEntry) == 96, "BTLOCEntry must be exactly 96 bytes");
#pragma pack(pop)

} // namespace Blackthorn::Localization
