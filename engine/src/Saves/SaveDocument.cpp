#include "Saves/SaveDocument.h"

#include <cstring>

#include "Debug/Logger.h"
#include "Saves/Compression/ICompressor.h"
#include "Saves/Encryption/IEncryptor.h"

namespace Blackthorn::Saves {

void FileHeader::serialize(IO::ByteBuffer& buf) const {
	buf.writeU32(magic);
	buf.writeU16(formatVersion);
	buf.writeU16(flags);
	buf.writeU64(saveIdHash);
	buf.writeU64(createdAt);
	buf.writeU64(updatedAt);
	buf.writeU64(payloadSize);
	buf.writeU64(plaintextSize);
	buf.writeU64(checksum);
	buf.writeU64(reserved);
}

void FileHeader::deserialize(IO::ByteBuffer& buf) {
	magic = buf.readU32();
	formatVersion = buf.readU16();
	flags = buf.readU16();
	saveIdHash = buf.readU64();
	createdAt = buf.readU64();
	updatedAt = buf.readU64();
	payloadSize = buf.readU64();
	plaintextSize = buf.readU64();
	checksum = buf.readU64();
	reserved = buf.readU64();
}

void EncryptionHeader::serialize(IO::ByteBuffer& buf) const {
	buf.writeBytes(nonce, NONCE_SIZE);
	buf.writeBytes(authTag, AUTH_TAG_SIZE);
}

void EncryptionHeader::deserialize(IO::ByteBuffer& buf) {
	buf.readBytes(nonce, NONCE_SIZE);
	buf.readBytes(authTag, AUTH_TAG_SIZE);
}

void SectionTableEntry::serialize(IO::ByteBuffer& buf) const {
	buf.writeU64(sectionId);
	buf.writeU64(offset);
	buf.writeU64(size);
	buf.writeU32(version);
	buf.writeU32(flags);
}

void SectionTableEntry::deserialize(IO::ByteBuffer& buf) {
	sectionId = buf.readU64();
	offset = buf.readU64();
	size = buf.readU64();
	version = buf.readU32();
	flags = buf.readU32();
}

void SaveIdBlock::fromSaveId(const SaveId& id) {
	displayName = id.displayName;
	worldId = id.worldId;
	playerId = id.playerId;
	slot = id.slot;
	flags = static_cast<U32>(id.flags);
}

void SaveIdBlock::applyToSaveId(SaveId& id) const {
	id.displayName = displayName;
	id.worldId = worldId;
	id.playerId = playerId;
	id.slot = slot;
	id.flags = static_cast<SaveFlags>(flags);
}

void SaveIdBlock::serialize(IO::ByteBuffer& buf) const {
	buf.writeString(displayName);
	buf.writeString(worldId);
	buf.writeString(playerId);
	buf.writeU32(slot);
	buf.writeU32(flags);
}

void SaveIdBlock::deserialize(IO::ByteBuffer& buf) {
	displayName = buf.readString();
	worldId = buf.readString();
	playerId = buf.readString();
	slot = buf.readU32();
	flags = buf.readU32();
}

U64 SaveDocument::computeChecksum(const U8* data, size_t size) noexcept {
	U64 hash = 14695981039346656037ULL;
	for (size_t i = 0; i < size; ++i) {
		hash ^= static_cast<U64>(data[i]);
		hash *= 1099511628211ULL;
	}

	return hash;
}

void SaveDocument::beginWrite(const SaveId& saveId) {
	header = FileHeader{};
	encHeader = EncryptionHeader{};
	sectionTable.clear();
	plaintextPayload.clear();

	header.saveIdHash = computeChecksum(
		saveId.id.bytes.data(), saveId.id.bytes.size()
	);

	header.createdAt = saveId.createdAt;
	header.updatedAt = saveId.updatedAt;

	saveIdBlock.fromSaveId(saveId);
}

void SaveDocument::addSection(
	U64 sectionId,
	U32 version,
	const IO::ByteBuffer& data
) {
	SectionTableEntry entry;
	entry.sectionId = sectionId;
	entry.offset = static_cast<U64>(plaintextPayload.size());
	entry.size = static_cast<U64>(data.size());
	entry.version = version;
	entry.flags = 0;

	sectionTable.push_back(entry);
	plaintextPayload.writeBytes(data.data(), data.size());
}

IO::ByteBuffer SaveDocument::finalise(
	ICompressor* compressor,
	IEncryptor* encryptor,
	const U8 outKey[32]
) {
	IO::ByteBuffer compressed;

	if (compressor && plaintextPayload.size() > 0) {
		if (compressor->compress(plaintextPayload, compressed)) {
			header.flags |= static_cast<U16>(DocumentFlags::Compressed);
		} else {
			BT_WARN("SaveDocument: compression failed, writing uncompressed");
			compressed = plaintextPayload;
		}
	} else {
		compressed = plaintextPayload;
	}

	header.plaintextSize = static_cast<U64>(plaintextPayload.size());

	IO::ByteBuffer payload;
	bool didEncrypt = false;

	if (encryptor && outKey) {
		if (encryptor->encrypt(compressed, payload, outKey,
				encHeader.nonce, encHeader.authTag)) {
			didEncrypt = true;
			header.flags |= static_cast<U16>(DocumentFlags::Encrypted);
		} else {
			BT_WARN("SaveDocument: encryption failed, writing unencrypted");
			payload = std::move(compressed);
		}
	} else {
		payload = std::move(compressed);
	}

	header.payloadSize = static_cast<U64>(payload.size());
	header.checksum = computeChecksum(payload.data(), payload.size());

	IO::ByteBuffer out;
	out.reserve(
		FileHeader::SERIALIZED_SIZE
		+ (didEncrypt ? EncryptionHeader::SERIALIZED_SIZE : 0)
		+ sizeof(U32)
		+ sectionTable.size() * SectionTableEntry::SERIALIZED_SIZE
		+ payload.size()
	);

	header.serialize(out);

	if (didEncrypt)
		encHeader.serialize(out);

	out.writeU32(static_cast<U32>(sectionTable.size()));
	for (const auto& entry : sectionTable)
		entry.serialize(out);

	{
		IO::ByteBuffer blockBuf;
		saveIdBlock.serialize(blockBuf);
		out.writeU32(static_cast<U32>(blockBuf.size()));
		out.writeBytes(blockBuf.data(), blockBuf.size());
	}

	out.writeBytes(payload.data(), payload.size());

	return out;
}

bool SaveDocument::parse(const IO::ByteBuffer& raw) {
	rawBytes = raw;
	rawBytes.resetRead();

	if (rawBytes.remaining() < FileHeader::SERIALIZED_SIZE) {
		BT_WARN("SaveDocument: file too small to contain a valid header");
		return false;
	}

	header.deserialize(rawBytes);

	if (!header.isValid()) {
		BT_WARN("SaveDocument: invalid magic number, not a Blackthorn save file");
		return false;
	}

	const bool encrypted = hasFlag(
		static_cast<DocumentFlags>(header.flags), DocumentFlags::Encrypted);

	if (encrypted) {
		if (rawBytes.remaining() < EncryptionHeader::SERIALIZED_SIZE) {
			BT_WARN("SaveDocument: file truncated before encryption header");
			return false;
		}

		encHeader.deserialize(rawBytes);
	}

	if (rawBytes.remaining() < sizeof(U32)) {
		BT_WARN("SaveDocument: file truncated before section count");
		return false;
	}

	const U32 count = rawBytes.readU32();
	sectionTable.clear();
	sectionTable.reserve(count);

	const size_t tableBytes = count * SectionTableEntry::SERIALIZED_SIZE;
	if (rawBytes.remaining() < tableBytes) {
		BT_WARN("SaveDocument: file truncated in section table");
		return false;
	}

	for (U32 i = 0; i < count; ++i) {
		SectionTableEntry entry;
		entry.deserialize(rawBytes);
		sectionTable.push_back(entry);
	}

	if (rawBytes.remaining() >= sizeof(U32)) {
		const U32 blockSize = rawBytes.readU32();
		if (blockSize > 0 && rawBytes.remaining() >= blockSize) {
			const size_t blockStart = rawBytes.readPosition();
			saveIdBlock.deserialize(rawBytes);
			const size_t consumed = rawBytes.readPosition() - blockStart;

			if (consumed < blockSize)
				rawBytes.skip(blockSize - consumed);
		} else if (blockSize > 0) {
			BT_WARN("SaveDocument: SaveIdBlock truncated in file");
			return false;
		}
	}

	const size_t payloadOffset = rawBytes.readPosition();
	if (rawBytes.remaining() < header.payloadSize) {
		BT_WARN("SaveDocument: file truncated in payload");
		return false;
	}

	const U64 actualChecksum = computeChecksum(
		rawBytes.data() + payloadOffset,
		static_cast<size_t>(header.payloadSize)
	);

	if (actualChecksum != header.checksum) {
		BT_WARN("SaveDocument: checksum mismatch, file may be corrupted");
		return false;
	}

	return true;
}

IO::ByteBuffer SaveDocument::decryptAndDecompress(
	IEncryptor* encryptor,
	ICompressor* compressor,
	const U8 inKey[32]
) const {

	const bool encrypted = hasFlag(
		static_cast<DocumentFlags>(header.flags), DocumentFlags::Encrypted
	);

	const bool compressed = hasFlag(
		static_cast<DocumentFlags>(header.flags), DocumentFlags::Compressed
	);

	size_t payloadStart = FileHeader::SERIALIZED_SIZE;
	if (encrypted)
		payloadStart += EncryptionHeader::SERIALIZED_SIZE;

	IO::ByteBuffer tmp(rawBytes.data(), rawBytes.size());
	tmp.skip(payloadStart);
	const U32 sectionCount = tmp.readU32();
	payloadStart += sizeof(U32) + sectionCount * SectionTableEntry::SERIALIZED_SIZE;

	tmp.skip(sectionCount * SectionTableEntry::SERIALIZED_SIZE);
	if (tmp.remaining() >= sizeof(U32)) {
		const U32 blockSize = tmp.readU32();
		payloadStart += sizeof(U32) + blockSize;
		tmp.skip(blockSize);
	}

	IO::ByteBuffer payload(
		rawBytes.data() + payloadStart,
		static_cast<size_t>(header.payloadSize)
	);

	IO::ByteBuffer decrypted;
	if (encrypted) {
		if (!encryptor) {
			BT_ERROR("SaveDocument: document is encrypted but no encryptor provided");
			return {};
		}

		if (!encryptor->decrypt(payload, decrypted, inKey,
				encHeader.nonce, encHeader.authTag)
		) {
			BT_ERROR("SaveDocument: decryption failed, wrong key or corrupted file");
			return {};
		}
	} else {
		decrypted = std::move(payload);
	}

	IO::ByteBuffer plaintext;
	if (compressed) {
		if (!compressor) {
			BT_ERROR("SaveDocument: document is compressed but no compressor provided");
			return {};
		}

		if (!compressor->decompress(decrypted, plaintext,
				static_cast<size_t>(header.plaintextSize))
		) {
			BT_ERROR("SaveDocument: decompression failed");
			return {};
		}
	} else {
		plaintext = std::move(decrypted);
	}

	return plaintext;
}

IO::ByteBuffer SaveDocument::getSectionData(
	U64 sectionId,
	const IO::ByteBuffer& payload,
	U32& outVersion
) const {
	for (const auto& entry : sectionTable) {
		if (entry.sectionId != sectionId)
			continue;

		if (entry.offset + entry.size > payload.size()) {
			BT_ERROR("SaveDocument: section data out of bounds in payload");
			return {};
		}

		outVersion = entry.version;
		return IO::ByteBuffer(
			payload.data() + entry.offset,
			static_cast<size_t>(entry.size)
		);
	}

	return {};
}

bool SaveDocument::hasSection(U64 sectionId) const noexcept {
	for (const auto& entry : sectionTable)
		if (entry.sectionId == sectionId)
			return true;

	return false;
}

} // namespace Blackthorn::Saves