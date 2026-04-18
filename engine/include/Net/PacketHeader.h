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
	ConnectRequest = 0x0, ///< Connection request.
	ConnectAck = 0x01, ///< Connection acknowledgement.
	Disconnect = 0x02, ///< Graceful disconnect notification.

	Heartbeat = 0x03, ///< Keep-alive with no payload.
	HeartbeatAck = 0x4,

	Snapshot = 0x10, ///< Full or delta entity snapshot (raw binary payload).
	Input = 0x11, ///< Client input stream (bit-packed payload).
	Message = 0x12, ///< Tagged message (spawn, despawn, ability, UI event).
};

/**
 * @brief Packet-level flags carried in `PacketHeader::flags`.
 *
 * Individual bits — combine with bitwise OR, clear with `clearFlag()` or
 * `flags &= ~flag`.
 *
 * @note `Compressed` and `Encrypted` are reserved for future transport
 * implementations. The engine sets these bits but does not perform
 * compression or encryption itself — that responsibility belongs to the
 * transport layer wrapping the raw ByteBuffer.
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

inline PacketFlags operator&(PacketFlags a, PacketFlags b) {
	return static_cast<PacketFlags>(
		static_cast<Uint8>(a) & static_cast<Uint8>(b)
	);
}

inline PacketFlags operator~(PacketFlags a) {
	return static_cast<PacketFlags>(
		static_cast<Uint8>(~static_cast<Uint8>(a))
	);
}

inline PacketFlags& operator|=(PacketFlags& a, PacketFlags b) {
	a = a | b;
	return a;
}

inline PacketFlags& operator&=(PacketFlags& a, PacketFlags b) {
	a = a & b;
	return a;
}
/** @brief Returns true if `flags` contains `flag`. */
inline bool hasFlag(PacketFlags flags, PacketFlags flag) {
	return (static_cast<Uint8>(flags) & static_cast<Uint8>(flag)) != 0;
}

/** @brief Returns `flags` with `flag` cleared. */
inline PacketFlags clearFlag(PacketFlags flags, PacketFlags flag) {
	return static_cast<PacketFlags>(
		static_cast<Uint8>(flags) & ~static_cast<Uint8>(flag)
	);
}

/**
 * @brief Fixed 24-byte header written at the start of every packet.
 *
 * Layout (little-endian, all fields mandatory):
 * @code
 * Offset  Size  Field
 *      0     8  tick           (SimClock tick at time of send)
 *      8     4  magic          (0x424C4B54 == "BLKT")
 *     12     4  reserved
 *     16     4  payloadLength  (bytes following this header)
 *     20     2  schemaVersion  (bumped on any breaking wire format change)
 *     22     1  packetType     (PacketType enum)
 *     23     1  flags          (PacketFlags bitmask)
 * @endcode
 * Total: 24 bytes.
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
	Uint32 reserved = 0;
	Uint32 payloadLength = 0;
	Uint16 schemaVersion = CURRENT_SCHEMA_VERSION;
	PacketType packetType = PacketType::Heartbeat;
	PacketFlags flags = PacketFlags::None;

	/**
	 * @brief Serializes the header into `buf` in the fixed 24-byte layout.
	 * @param buf Destination buffer.
	 */
	void serialize(ByteBuffer& buf) const {
		buf.writeU32(magic);
		buf.writeU16(schemaVersion);
		buf.writeU32(payloadLength);
		buf.writeU64(tick);
		buf.writeU8(static_cast<Uint8>(packetType));
		buf.writeU8(static_cast<Uint8>(flags));
		buf.writeU32(reserved);
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
		payloadLength = buf.readU32();
		tick = buf.readU64();
		packetType = static_cast<PacketType>(buf.readU8());
		flags = static_cast<PacketFlags>(buf.readU8());
		reserved = buf.readU32();
	}

	/**
	 * @brief Returns true if the magic constant matches and the schema
	 * version is the one this build understands.
	 */
	bool isValid() const {
		return magic == MAGIC && schemaVersion == CURRENT_SCHEMA_VERSION;
	}
};

static_assert(
	sizeof(Uint64) +
	sizeof(Uint32) * 3 +
	sizeof(Uint16) +
	sizeof(Uint8) * 2
	== PacketHeader::SERIALIZED_SIZE,
	"PacketHeader: serialized field sizes do not sum to SERIALIZED_SIZE"
);

} // namespace Blackthorn::Net