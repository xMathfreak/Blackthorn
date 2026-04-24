#pragma once

#include "Core/Export.h"
#include "Net/Core/ByteBuffer.h"

namespace Blackthorn::Net::Protocol {

/**
 * @brief Wire header inserted between @c UDPHeader and @c PacketHeader in
 * every UDP datagram, enabling transparent fragmentation of large payloads.
 *
 * @par Wire layout
 *
 * Every UDP datagram after the @c UDPHeader begins with a 1-byte flags value:
 *
 * @code
 * [UDPHeader   8 bytes]     — sequence numbers and ACK bitmask
 * [flags       1 byte ]     — bit 0 = isFragmented; remaining bits reserved
 * @endcode
 *
 * If @c isFragmented is @b false (flags byte == 0), the datagram is
 * unfragmented and the @c PacketHeader follows immediately. Total overhead
 * for a non-fragmented packet: 1 byte (just the flags).
 *
 * If @c isFragmented is @b true, four additional bytes follow the flags:
 *
 * @code
 * [flags       1 byte ]     — 0x01 = fragmented
 * [fragmentId  2 bytes]     — which logical message this belongs to
 * [totalFrags  1 byte ]     — total fragment count (1–255)
 * [fragIndex   1 byte ]     — 0-based index of this fragment (0 to totalFrags-1)
 * @endcode
 *
 * @c PacketHeader is present only in fragment 0 (the first fragment).
 * Subsequent fragments carry only raw payload bytes after the 5-byte header.
 *
 * @par Maximum message size
 *
 * With a practical MTU of 1400 bytes and the combined overhead of
 * @c UDPHeader (8) + @c FragmentHeader (5) + @c PacketHeader (12) = 25 bytes
 * in fragment 0, and 13 bytes in subsequent fragments, the maximum reassembled
 * message is approximately:
 *
 *   fragment 0:      1400 - 25 = 1375 bytes of payload
 *   fragments 1–254: 1400 - 13 = 1387 bytes of payload each
 *   total:           1375 + 254 * 1387 ≈ 353 KB
 *
 * If larger messages are needed, TCP should be used instead.
 */

struct BLACKTHORN_API FragmentHeader {
	/// Size of the fragmented form of this header (flags + id + total + index).
	static constexpr size_t FRAGMENTED_SIZE = 5;

	/// Size when the packet is not fragmented (flags byte only).
	static constexpr size_t UNFRAGMENTED_SIZE = 1;

	/// Bit flag indicating this datagram is part of a fragmented message.
	static constexpr Uint8 FLAG_FRAGMENTED = 0x01u;

	/// Maximum number of fragments per message.
	static constexpr Uint8 MAX_FRAGMENTS = 255;

	Uint16 fragmentId = 0; ///< Logical message ID (per-peer, wrapping).
	Uint8 flags = 0; ///< Bitmask; bit 0 = isFragmented;
	Uint8 totalFrags = 1; ///< Total fragments in this message.
	Uint8 fragIndex = 0; ///< 0-based index of this fragment.

	bool isFragmented() const noexcept {
		return (flags & FLAG_FRAGMENTED) != 0;
	}

	/**
	 * @brief Serializes the header.
	 *
	 * Always writes at least the 1 byte flags. If @c isFragmented() is
	 * true, writes the remaining 4 bytes as well.
	 */
	void serialize(Core::ByteBuffer& buf) const {
		buf.writeU8(flags);
		if (isFragmented()) {
			buf.writeU16(fragmentId);
			buf.writeU8(totalFrags);
			buf.writeU8(fragIndex);
		}
	}

	/**
	 * @brief Deserializes the header from @p buf.
	 *
	 * Reads the flags byte first. If the fragment bit is set, reads
	 * the remaining 4 bytes. The caller must verify sufficient
	 * remaining bytes before calling.
	 */
	void deserialize(Core::ByteBuffer& buf) {
		flags = buf.readU8();
		if (isFragmented()) {
			fragmentId = buf.readU16();
			totalFrags = buf.readU8();
			fragIndex = buf.readU8();
		}
	}
};

} //namespace Blackthorn::Net::Protocol