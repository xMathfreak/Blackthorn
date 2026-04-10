#pragma once

#include <SDL3/SDL.h>

#include "Core/Export.h"
#include "Net/ByteBuffer.h"

namespace Blackthorn::Net {

/**
 * @brief Packet types carried in `PacketHeader::packetType`.
 *
 * Both the engine and server must agree on these values. Adding new types
 * is additive — existing values must never be renumbered.
 */
enum class PacketType : Uint8 {
	Snapshot = 0x01, ///< Full or delta entity snapshot (raw binary payload).
	Input = 0x02, ///< Client input stream (bit-packed payload).
	Message = 0x03, ///< Tagged message (spawn, despawn, ability, UI event).
	Heartbeat = 0x04, ///< Keep-alive with no payload.
	Disconnect = 0x05, ///< Graceful disconnect notification.
};

/**
 * @brief Packet-level flags carried in `PacketHeader::flags`.
 *
 * Individual bits. Combine with bitwise OR.
 */
enum class PacketFlags : Uint8 {
	None = 0x00,
	Compressed = 0x01, ///< Payload is compressed
	Encrypted = 0x02, ///< Payload is encrypted
	Reliable = 0x04, ///< Transport should guarantee delivery.
	Fragment = 0x08, ///< This packet is part of a fragmented sequence.
};

inline PacketFlags operator|(PacketFlags a, PacketFlags b) {
	return static_cast<PacketFlags>(
		static_cast<Uint8>(a) | static_cast<Uint8>(b)
	);
}

inline bool hasFlag(PacketFlags flags, PacketFlags flag) {
	return (static_cast<Uint8>(flags) & static_cast<Uint8>(flag)) != 0;
}

/**
 * @brief Fixed 20-byte header written at the start of every packet.
 *
 * Layout (little-endian, all fields mandatory):
 * @code
 * Offset  Size  Field
 *      0     4  magic          (0x424C4B54 == "BLKT")
 *      4     2  schemaVersion  (bumped on any breaking wire format change)
 *      6     1  packetType     (PacketType enum)
 *      7     1  flags          (PacketFlags bitmask)
 *      8     8  tick           (SimClock tick at time of send)
 *     16     4  payloadLength  (bytes following this header)
 * @endcode
 * Total: 20 bytes.
 *
 * The magic constant lets receivers quickly reject garbage data without
 * attempting to interpret it. `schemaVersion` gates the payload parser —
 * if the version is unrecognised the packet is dropped before any payload
 * bytes are read.
 */
struct BLACKTHORN_API PacketHeader {
	static constexpr Uint32 MAGIC = 0x424C4B54u; // "BLKT"
	static constexpr size_t SERIALIZED_SIZE = 24;

	/// Current schema version. Bump this whenever the wire format changes
	/// in a way that is not backwards compatible.
	static constexpr Uint16 CURRENT_SCHEMA_VERSION = 1;

	Uint64 tick = 0;
	Uint32 magic = MAGIC;
	Uint32 payloadLength = 0;
	Uint16 schemaVersion = CURRENT_SCHEMA_VERSION;
	PacketType packetType = PacketType::Heartbeat;
	PacketFlags flags = PacketFlags::None;

	/**
	 * @brief Serializes the header into `buf` in the fixed 20-byte layout.
	 * @param buf Destination buffer.
	 */
	void serialize(ByteBuffer& buf) const {
		buf.writeU32(magic);
		buf.writeU16(schemaVersion);
		buf.writeU8(static_cast<Uint8>(packetType));
		buf.writeU8(static_cast<Uint8>(flags));
		buf.writeU64(tick);
		buf.writeU32(payloadLength);
	}

	/**
	 * @brief Deserializes a header from `buf`.
	 *
	 * Does not validate the magic value or schema version. Call
	 * `isValid()` after deserializing to check both.
	 *
	 * @param buf Source buffer positioned at the start of the header.
	 */
	void deserialize(ByteBuffer& buf) {
		magic = buf.readU32();
		schemaVersion = buf.readU16();
		packetType = static_cast<PacketType>(buf.readU8());
		flags = static_cast<PacketFlags>(buf.readU8());
		tick = buf.readU64();
		payloadLength = buf.readU32();
	}

	/**
	 * @brief Returns true if the magic constant matches and the schema
	 * version is the one this build understands.
	 */
	bool isValid() const {
		return magic == MAGIC && schemaVersion == CURRENT_SCHEMA_VERSION;
	}
};

static_assert(sizeof(PacketHeader) == PacketHeader::SERIALIZED_SIZE, "PacketHeader::SERIALIZED_SIZE does not match sizeof(PacketHeader)");

} // namespace Blackthorn::Net