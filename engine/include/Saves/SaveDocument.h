#pragma once

#include <vector>

#include "Core/Export.h"
#include "IO/ByteBuffer.h"
#include "Saves/SaveId.h"

namespace Blackthorn::Saves {

// Forward declarations
class ICompressor;
class IEncryptor;
class ISaveSection;

/// Engine format version. Increment when the FileHeader or SectionTable
/// layout changes in a breaking way.
static constexpr U16 SAVE_FORMAT_VERSION = 1;

/// Magic number written at byte 0 of every save file: "BTSV"
static constexpr U32 SAVE_MAGIC = 0x42545356u;

/**
 * @brief Flags stored in @c FileHeader::flags describing the payload encoding.
 */
enum class DocumentFlags : U16 {
	None = 0,
	Compressed = 1 << 0, ///< Payload is zstd compressed.
	Encrypted = 1 << 1, ///< Payload is XChaCha20-Poly1305 encrypted.
};

inline DocumentFlags operator|(DocumentFlags a, DocumentFlags b) {
	return static_cast<DocumentFlags>(
		static_cast<U16>(a) | static_cast<U16>(b));
}

inline bool hasFlag(DocumentFlags flags, DocumentFlags flag) {
	return (static_cast<U16>(flags) & static_cast<U16>(flag)) != 0;
}

/**
 * @brief Fixed-size 64-byte file header. Always unencrypted and uncompressed.
 *
 * Wire layout (little-endian):
 * @code
 * Offset  Size  Field
 *      0     4  magic           (0x424C4B53 "BTSV")
 *      4     2  formatVersion
 *      6     2  flags           (DocumentFlags bitmask)
 *      8     8  saveIdHash      (FNV-1a of SaveId.id.bytes)
 *     16     8  createdAt       (unix ms)
 *     24     8  updatedAt       (unix ms)
 *     32     8  payloadSize     (bytes of the compressed+encrypted blob)
 *     40     8  plaintextSize   (bytes after decompression, for pre-allocation)
 *     48     8  checksum        (FNV-1a of the compressed+encrypted payload bytes)
 *     56     8  reserved
 * Total: 64 bytes
 * @endcode
 *
 * @note The nonce and authentication tag for encryption are stored in the
 * @c EncryptionHeader that immediately follows the @c FileHeader when
 * @c DocumentFlags::Encrypted is set. Keeping them separate avoids
 * complicating the fixed-size header with variable-length fields.
 */
struct BLACKTHORN_API FileHeader {
	static constexpr size_t SERIALIZED_SIZE = 64;

	U32 magic = SAVE_MAGIC;
	U16 formatVersion = SAVE_FORMAT_VERSION;
	U16 flags = 0;
	U64 saveIdHash = 0;
	U64 createdAt = 0;
	U64 updatedAt = 0;
	U64 payloadSize = 0;
	U64 plaintextSize = 0;
	U64 checksum = 0;
	U64 reserved = 0;

	void serialize(IO::ByteBuffer& buf) const;
	void deserialize(IO::ByteBuffer& buf);

	bool isValid() const noexcept { return magic == SAVE_MAGIC; }
};

static_assert(
	sizeof(U32) + sizeof(U16) * 2 + sizeof(U64) * 7
	== FileHeader::SERIALIZED_SIZE,
	"FileHeader: Serialized field sizes do not match SERIALIZED_SIZE"
);

/**
 * @brief Encryption metadata stored immediately after @c FileHeader when
 * @c DocumentFlags::Encrypted is set. Always unencrypted.
 *
 * Wire layout:
 * @code
 * Offset  Size  Field
 *      0    24  nonce     (XChaCha20-Poly1305 nonce, randomly generated per save)
 *     24    16  authTag   (Poly1305 authentication tag)
 * Total: 40 bytes
 * @endcode
 */
struct BLACKTHORN_API EncryptionHeader {
	static constexpr size_t NONCE_SIZE = 24;
	static constexpr size_t AUTH_TAG_SIZE = 16;
	static constexpr size_t SERIALIZED_SIZE = NONCE_SIZE + AUTH_TAG_SIZE;

	U8 nonce[NONCE_SIZE]{};
	U8 authTag[AUTH_TAG_SIZE]{};

	void serialize(IO::ByteBuffer& buf) const;
	void deserialize(IO::ByteBuffer& buf);
};

/**
 * @brief Unencrypted metadata block written immediately after the section
 * table, before the encrypted payload.
 *
 * Stores the @c SaveId fields that are absent from @c FileHeader —
 * specifically the variable-length strings and flag/slot values that
 * @c ISaveStorage::list() needs to filter saves without decrypting payloads.
 *
 * A @c U32 byte-length prefix is written before the block so that future
 * format versions can extend or replace it while older readers skip it safely.
 *
 * Wire layout (after the U32 length prefix):
 * @code
 *  [U16 + N bytes]  displayName   (ByteBuffer::writeString)
 *  [U16 + N bytes]  worldId
 *  [U16 + N bytes]  playerId
 *  [U32]            slot
 *  [U32]            flags         (SaveFlags bitmask)
 * @endcode
 */
struct BLACKTHORN_API SaveIdBlock {
	std::string displayName;
	std::string worldId;
	std::string playerId;
	U32 slot  = 0;
	U32 flags = 0;

	void serialize(IO::ByteBuffer& buf) const;
	void deserialize(IO::ByteBuffer& buf);

	/** @brief Populates this block from a @c SaveId. */
	void fromSaveId(const SaveId& id);

	/** @brief Applies all fields into @p id, leaving id.id/createdAt/updatedAt untouched. */
	void applyToSaveId(SaveId& id) const;
};

/**
 * @brief One entry in the section table.
 *
 * The section table is stored after the file header (and optional encryption
 * header), unencrypted, so the storage layer can inspect section presence
 * without decrypting the payload.
 *
 * Wire layout per entry:
 * @code
 *  0   8  sectionId   (FNV-1a hash of section name)
 *  8   8  offset      (byte offset into the decrypted+decompressed payload)
 * 16   8  size        (byte count of section data in the payload)
 * 24   4  version     (section schema version)
 * 28   4  flags       (reserved, must be 0)
 * Total: 32 bytes per entry
 * @endcode
 */
struct BLACKTHORN_API SectionTableEntry {
	static constexpr size_t SERIALIZED_SIZE = 32;

	U64 sectionId = 0;
	U64 offset = 0;
	U64 size = 0;
	U32 version = 0;
	U32 flags = 0;

	void serialize(IO::ByteBuffer& buf) const;
	void deserialize(IO::ByteBuffer& buf);
};

/**
 * @brief Assembles and parses a complete save document binary blob.
 *
 * @c SaveDocument is responsible for the binary layout only — it does not
 * own sections or know what the section data means. @c SaveManager drives
 * the section write/read callbacks and passes the raw bytes here.
 *
 * @par Writing
 * @code
 * SaveDocument doc;
 * doc.beginWrite(saveId);
 * doc.addSection(sectionId, version, payloadBytes);
 * doc.addSection(...);
 * auto bytes = doc.finalize(compressor, encryptor, keyDeriveFn, saveId);
 * @endcode
 *
 * @par Reading
 * @code
 * SaveDocument doc;
 * doc.parse(rawBytes);                              // validates header + section table
 * auto payload = doc.decryptAndDecompress(encryptor, compressor, keyDeriveFn, saveId);
 * auto sectionData = doc.getSectionData(sectionId, payload);
 * @endcode
 */
class BLACKTHORN_API SaveDocument {
public:
	SaveDocument() = default;

	SaveDocument(const SaveDocument&) = delete;
	SaveDocument& operator=(const SaveDocument&) = delete;

	/**
	 * @brief Initializes the document for writing with identity from @p saveId.
	 */
	void beginWrite(const SaveId& saveId);

	/**
	 * @brief Appends a section payload to the document.
	 * @param sectionId FNV-1a hash of the section name.
	 * @param version   Section schema version.
	 * @param data      Raw section payload bytes.
	 */
	void addSection(U64 sectionId, U32 version, const IO::ByteBuffer& data);

	/**
	 * @brief Compresses, encrypts, and serializes the document to bytes.
	 *
	 * @param compressor  Compressor to apply before encryption. May be null
	 *                    (no compression).
	 * @param encryptor   Encryptor to apply after compression. May be null
	 *                    (no encryption). If null and compressor is also null,
	 *                    the raw payload is written with no encoding flags set.
	 * @param outKey      32-byte encryption key. Ignored if encryptor is null.
	 * @return Serialized document bytes ready to pass to @c ISaveStorage::write().
	 */
	IO::ByteBuffer finalize(
		ICompressor* compressor,
		IEncryptor* encryptor,
		const U8 outKey[32]
	);

	/**
	 * @brief Parses the file header and section table from raw bytes.
	 *
	 * Does not decrypt or decompress the payload. Call this first to get
	 * the section list for compatibility checks.
	 *
	 * @param raw Raw bytes from storage.
	 * @return true if the header is valid (magic matches, checksum passes).
	 */
	bool parse(const IO::ByteBuffer& raw);

	/**
	 * @brief Decrypts and decompresses the payload after a successful @c parse().
	 *
	 * @param encryptor  Must match what was used during @c finalize(). Null if
	 *                   the document was not encrypted.
	 * @param compressor Must match what was used during @c finalize(). Null if
	 *                   the document was not compressed.
	 * @param inKey      32-byte decryption key. Ignored if encryptor is null.
	 * @return Decrypted and decompressed payload buffer, or empty on failure.
	 */
	IO::ByteBuffer decryptAndDecompress(
		IEncryptor* encryptor,
		ICompressor* compressor,
		const U8 inKey[32]
	) const;

	/**
	 * @brief Extracts one section's bytes from an already-decoded payload.
	 *
	 * @param sectionId FNV-1a hash of the section name to find.
	 * @param payload   The buffer returned by @c decryptAndDecompress().
	 * @param outVersion Receives the section schema version on success.
	 * @return Buffer containing only that section's bytes, or empty if not found.
	 */
	IO::ByteBuffer getSectionData(
		U64 sectionId,
		const IO::ByteBuffer& payload,
		U32& outVersion
	) const;

	const FileHeader& getHeader() const { return header; }

	/** @brief Returns the save identity block parsed by @c parse(). */
	const SaveIdBlock& getSaveIdBlock() const { return saveIdBlock; }

	/** @brief Returns all section table entries parsed by @c parse(). */
	const std::vector<SectionTableEntry>& getSectionTable() const { return sectionTable; }

	/** @brief Returns true if the section table contains the given ID. */
	bool hasSection(U64 sectionId) const noexcept;

private:
	FileHeader header;
	EncryptionHeader encHeader;
	std::vector<SectionTableEntry> sectionTable;

	// Accumulated plaintext payload during write, before compression/encryption.
	IO::ByteBuffer plaintextPayload;

	// Cached raw bytes from the last parse(), needed for decryptAndDecompress().
	IO::ByteBuffer rawBytes;

	// Save identity metadata read during parse(), used by list().
	SaveIdBlock saveIdBlock;

	static U64 computeChecksum(const U8* data, size_t size) noexcept;
};

} // namespace Blackthorn::Saves