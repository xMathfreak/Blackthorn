#pragma once

#include <SDL3/SDL.h>

#include "Core/Export.h"
#include "Net/Core/ByteBuffer.h"

namespace Blackthorn::Net::Protocol {

/**
 * @brief Packet types carried in `PacketHeader::packetType`.
 *
 * Both the engine and server must agree on these values. Adding new types
 * is additive - existing values must never be renumbered.
 */
enum class PacketType : Uint8 {
	ConnectRequest = 0x0, ///< Connection request (carries local schema version).
	ConnectAck = 0x01, ///< Connection acknowledgement (carries accepted schema version).
	Disconnect = 0x02, ///< Graceful disconnect notification.

	Heartbeat = 0x03, ///< Keep-alive with no payload.
	HeartbeatAck = 0x4, ///< Heartbeat acknowledgement.

	UDPPortInfo = 0x5, ///< Info containing a UDP port. Must have a 16 bit payload.

	AuthRequest = 0x06, ///< Authentication Request.
	AuthResponse = 0x07, ///< Authentication Response.
	AuthToken = 0x08, ///< Session token for persistent connection.

	Snapshot = 0x10, ///< Full or delta entity snapshot (raw binary payload).
	Input = 0x11, ///< Client input stream (bit-packed payload).
	Message = 0x12, ///< Tagged message (spawn, despawn, ability, UI event).

	ChatMessage = 0x20, ///< General chat message.
	Whisper = 0x21, ///< Private message between players.
	SystemMessage = 0x22, ///< Message broadcasted by the system.

	FileRequest = 0x30, ///< Request for file or asset.
	FileData = 0x31, ///< File chunk data.
	FileAck = 0x32, ///< Acknowledgement for file chunk receipt.
	FileSync = 0x33, ///< File synchronization or patching.
	FileTransferComplete = 0x34, ///< Indicates the ending of a file transfer.

	Ping = 0x40, ///< Measure round-trip latency.
	Pong = 0x41, ///< Response to ping.
};

/**
 * @brief Packet-level flags carried in `PacketHeader::flags`.
 *
 * Individual bits - combine with bitwise OR, clear with `clearFlag()` or
 * `flags &= ~flag`.
 *
 * @note `Compressed` and `Encrypted` are reserved for future transport
 * implementations. The engine sets these bits but does not perform
 * compression or encryption itself - that responsibility belongs to the
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
 * @brief Schema version negotiated during the TCP handshake.
 *
 * Carried in the @c ConnectRequest and @c ConnectAck payloads,
 * then stored per-peer in @c NetworkPeer::negotiatedSchemaVersion.
 */
static constexpr Uint16 CURRENT_SCHEMA_VERSION = 1;

/**
 * @brief Fixed 12-byte header written at the start of every packet.
 *
 * Layout (little-endian, all fields mandatory):
 * @code
 * Offset  Size  Field
 *      0     2  magic          (0x4254 == "BT")
 *      2     2  payloadLength  (bytes following this header, max 65535)
 *      4     4  tick           (SimClock tick at time of send)
 *      8     1  packetType     (PacketType enum)
 *      9     1  flags          (PacketFlags bitmask)
 *     10     2  reserved       (Keeps alignment)
 * @endcode
 * Total: 12 bytes.
 *
 * @par Schema version
 * Schema version is negotiated once during the TCP handshake
 * (@c ConnectRequest / @c ConnectAck payloads) and stored per-peer
 * in @c NetworkPeer::negotiatedSchemaVersion.
 *
 * @par Tick
 * A 32 bit simulation tick counter. Wrap around is handled by
 * @c tickisNewer().
 *
 * @par Payload Length
 * Maximum value is 65535. TCP messages larger than this are rejected
 * by @c TCPChannel. UDP datagrams are constrained by the practical MTU
 * (1400 bytes) in @c UDPChannel.
 *
 */
struct BLACKTHORN_API PacketHeader {
	static constexpr Uint16 MAGIC = 0x4254u; // "BT"
	static constexpr size_t SERIALIZED_SIZE = 12;

	Uint16 magic = MAGIC;
	Uint16 payloadLength = 0;
	Uint32 tick = 0;
	PacketType packetType = PacketType::Heartbeat;
	PacketFlags flags = PacketFlags::None;
	Uint16 reserved = 0;

	/**
	 * @brief Serializes the header into `buf` in the fixed 12-byte layout.
	 * @param buf Destination buffer.
	 */
	void serialize(Core::ByteBuffer& buf) const {
		buf.writeU16(magic);
		buf.writeU16(payloadLength);
		buf.writeU32(tick);
		buf.writeU8(static_cast<Uint8>(packetType));
		buf.writeU8(static_cast<Uint8>(flags));
		buf.writeU16(reserved);
	}

	/**
	 * @brief Deserializes a header from `buf`.
	 *
	 * Does not validate the magic value. Call `isValid()`
	 * after deserializing to check.
	 *
	 * @param buf Source buffer positioned at the start of the header.
	 */
	void deserialize(Core::ByteBuffer& buf) {
		magic = buf.readU16();
		payloadLength = buf.readU16();
		tick = buf.readU32();
		packetType = static_cast<PacketType>(buf.readU8());
		flags = static_cast<PacketFlags>(buf.readU8());
		reserved = buf.readU16();
	}

	/**
	 * @brief Returns true if the magic constant matches.
	 */
	bool isValid() const {
		return magic == MAGIC;
	}
};

static_assert(
	sizeof(Uint32) * 1 +
	sizeof(Uint16) * 3 +
	sizeof(Uint8) * 2
	== PacketHeader::SERIALIZED_SIZE,
	"PacketHeader: Serialized field sizes do not sum to SERIALIZED_SIZE"
);

inline bool tickIsNewer(Uint32 a, Uint32 b) noexcept {
	return static_cast<Sint32>(a - b) > 0;
}

} // namespace Blackthorn::Net::Protocol