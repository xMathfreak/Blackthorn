#pragma once

#include <functional>
#include <stdexcept>
#include <string>

#include <SDL3/SDL.h>

#include "Core/Export.h"
#include "Net/Core/ByteBuffer.h"

namespace Blackthorn::Net::Protocol {

/**
 * @brief Writes a tagged message payload into a ByteBuffer.
 *
 * Each field is prefixed with a Uint16 tag and a Uint16 byte length,
 * allowing readers to skip unknown fields safely. A Uint32 message
 * schema version is written first so consumers can gate on it before
 * reading any fields.
 *
 * Wire layout of the payload written by this class:
 * @code
 * [uint32 messageVersion]
 * repeated:
 *   [uint16 tag]
 *   [uint16 fieldLength]
 *   [fieldLength bytes of field data]
 * @endcode
 *
 * This payload is intended to follow a `PacketHeader` in the full packet.
 *
 * @code
 * ByteBuffer buf;
 * MessageWriter writer(buf, 1);   // message schema version 1
 * writer.writeU32(Tags::EntityId, entityId);
 * writer.writeString(Tags::Name, "PlayerSpawn");
 * writer.writeF32(Tags::PositionX, x);
 * writer.writeF32(Tags::PositionY, y);
 * @endcode
 */
class BLACKTHORN_API MessageWriter {
public:
	/**
	 * @brief Constructs a MessageWriter and emits the version prefix.
	 * @param buf            Destination buffer.
	 * @param messageVersion Schema version of this specific message type.
	 */
	explicit MessageWriter(Core::ByteBuffer& buf, Uint32 messageVersion)
		: buf(buf)
	{
		buf.writeU32(messageVersion);
	}

	MessageWriter(const MessageWriter&) = delete;
	MessageWriter& operator=(const MessageWriter&) = delete;

	void writeU8(Uint16 tag, Uint8 v) {
		writeFieldHeader(tag, sizeof(Uint8));
		buf.writeU8(v);
	}

	void writeU16(Uint16 tag, Uint16 v) {
		writeFieldHeader(tag, sizeof(Uint16));
		buf.writeU16(v);
	}

	void writeU32(Uint16 tag, Uint32 v) {
		writeFieldHeader(tag, sizeof(Uint32));
		buf.writeU32(v);
	}

	void writeU64(Uint16 tag, Uint64 v) {
		writeFieldHeader(tag, sizeof(Uint64));
		buf.writeU64(v);
	}

	void writeI32(Uint16 tag, Sint32 v) {
		writeFieldHeader(tag, sizeof(Sint32));
		buf.writeI32(v);
	}

	void writeF32(Uint16 tag, float v) {
		writeFieldHeader(tag, sizeof(float));
		buf.writeF32(v);
	}

	void writeBool(Uint16 tag, bool v) {
		writeFieldHeader(tag, 1);
		buf.writeBool(v);
	}

	/**
	 * @brief Writes a string field. String length is limited to 65535 bytes
	 * by the inner writeString call; the outer field length covers the
	 * 2-byte length prefix plus the string data.
	 */
	void writeString(Uint16 tag, const std::string& v) {
		if (v.size() > 65533)
			throw std::length_error("MessageWriter::writeString: string too long");

		writeFieldHeader(tag, static_cast<Uint16>(2 + v.size()));
		buf.writeString(v);
	}

	/**
	 * @brief Writes an arbitrary byte blob as a field.
	 * @param tag  Field identifier.
	 * @param data Pointer to byte data.
	 * @param size Number of bytes. Must fit in a Uint16.
	 */
	void writeBlob(Uint16 tag, const Uint8* data, Uint16 size) {
		writeFieldHeader(tag, size);
		buf.writeBytes(data, size);
	}

private:
	Core::ByteBuffer& buf;

	void writeFieldHeader(Uint16 tag, Uint16 fieldLength) {
		buf.writeU16(tag);
		buf.writeU16(fieldLength);
	}
};

/**
 * @brief Reads a tagged message payload from a ByteBuffer.
 *
 * Iterates fields by tag. Unknown tags are skipped transparently using
 * the stored field length, making the format forward-compatible when new
 * fields are added to a message type.
 *
 * @code
 * MessageReader reader(buf);
 * if (reader.messageVersion() != EXPECTED_VERSION)
 *     return; // reject old/new format
 *
 * reader.read([&](Uint16 tag, ByteBuffer& field) {
 *     switch (tag) {
 *         case Tags::EntityId:   entityId = field.readU32(); break;
 *         case Tags::PositionX:  x        = field.readF32(); break;
 *         // Unknown tags are automatically skipped by the reader.
 *     }
 * });
 * @endcode
 */
class BLACKTHORN_API MessageReader {
public:
	/**
	 * @brief Constructs a MessageReader and reads the version prefix.
	 * @param buf Source buffer positioned at the start of the message payload.
	 */
	explicit MessageReader(Core::ByteBuffer& buf)
		: buf(buf)
		, version(buf.readU32())
	{}

	MessageReader(const MessageReader&) = delete;
	MessageReader& operator=(const MessageReader&) = delete;

	/** @brief Returns the message schema version written by the sender. */
	Uint32 messageVersion() const { return version; }

	/**
	 * @brief Iterates all fields, invoking `callback` for each.
	 *
	 * The callback receives the field tag and a ByteBuffer scoped to
	 * exactly the field's bytes. Any bytes not consumed by the callback
	 * are skipped before moving to the next field. This means partial
	 * reads of a field are safe - the reader will still advance correctly.
	 *
	 * @param callback Invocable as `void(Uint16 tag, ByteBuffer& field)`.
	 */
	void read(const std::function<void(Uint16, Core::ByteBuffer&)>& callback) {
		while (!buf.exhausted()) {
			// At minimum we need 4 bytes for tag + length.
			if (buf.remaining() < 4)
				break;

			Uint16 tag = buf.readU16();
			Uint16 fieldLength = buf.readU16();

			if (buf.remaining() < fieldLength)
				break;

			Core::ByteBuffer fieldView(buf.data() + buf.readPosition(), fieldLength);
			callback(tag, fieldView);

			skipBytes(fieldLength);
		}
	}

private:
	Core::ByteBuffer& buf;
	Uint32 version;

	void skipBytes(Uint16 count) {
		for (Uint16 i = 0; i < count; ++i)
			buf.readU8();
	}
};

} // namespace Blackthorn::Net::Protocol