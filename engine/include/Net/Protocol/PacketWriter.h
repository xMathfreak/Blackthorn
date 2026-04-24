#pragma once

#include <cassert>

#include "Net/Core/ByteBuffer.h"
#include "Net/Protocol/PacketHeader.h"

namespace Blackthorn::Net::Protocol {

/**
 * @brief RAII helper that builds a packet into a @c ByteBuffer and
 * automatically patches @c payloadLength when @c finish() is called.
 *
 * @details Solves problem of needing to write the header before you know
 * how many payload bytes will follow. The writer reserves header space at
 * construction time, lets you write arbitrary payload bytes, then
 * the @c payloadLength field in the already-written header
 * back-patches when the packet is complete.
 *
 * @par Usage
 * @code
 * ByteBuffer buf;
 * {
 *     PacketWriter pw(buf, PacketType::Snapshot, tick);
 *     pw.buffer().writeU32(entityCount);
 *     // ... write entity data ...
 *     pw.finish();
 * }
 * cm.broadcastUDP(buf);
 * @endcode
 *
 * @par Writing into an existing buffer
 * @c PacketWriter appends to whatever is already in @p buf. This allows
 * multiple packets to be concatenated into one @c ByteBuffer for batch
 * sends, or for a caller to prepend its own framing before the header.
 *
 * @par finish() must be called exactly once
 * If @c finish() is not called (e.g. the writer goes out of scope due to an
 * exception), the header @c payloadLength field will contain the placeholder
 * value 0 and the packet will fail @c poll()'s validation check. In debug
 * builds an assertion fires in the destructor if @c finish() was skipped.
 *
 * @note @c PacketWriter does not own the @c ByteBuffer. The caller is
 * responsible for keeping the buffer alive for the duration of the writer.
 */
class BLACKTHORN_API PacketWriter {
public:
	/**
	 * @brief Begins a new packet by serialising a @c PacketHeader into
	 * @p buf with @c payloadLength = 0.
	 *
	 * The header is written at the current end of @p buf. The write
	 * position of any existing data is not affected.
	 *
	 * @param buf        Destination buffer (appended to, not cleared).
	 * @param packetType Type field for the @c PacketHeader.
	 * @param tick       Simulation tick (default 0 for non-simulation packets).
	 * @param flags      Optional @c PacketFlags (default @c None).
	 */
	explicit PacketWriter(
		Core::ByteBuffer& buf,
		PacketType packetType,
		Uint64 tick = 0,
		PacketFlags flags = PacketFlags::None
	)
		: buf(buf)
		, headerOffset(buf.size())
	{
		PacketHeader hdr;
		hdr.packetType = packetType;
		hdr.tick = tick;
		hdr.flags = flags;
		hdr.payloadLength = 0;
		hdr.serialize(buf);

		payloadStart = buf.size();
	}

	PacketWriter(const PacketWriter&) = delete;
	PacketWriter& operator=(const PacketWriter&) = delete;

	PacketWriter(PacketWriter&&) = delete;
	PacketWriter& operator=(PacketWriter&&) = delete;

	~PacketWriter() {
		assert(finished && "PacketWriter destroyed without calling finish()");
	}

	/**
	 * @brief Returns a reference to the underlying @c ByteBuffer for
	 * writing payload bytes.
	 *
	 * @code
	 * PacketWriter pw(buf, PacketType::Input, tick);
	 * pw.buffer().writeU8(dx);
	 * pw.buffer().writeU8(dy);
	 * pw.finish();
	 * @endcode
	 */
	Core::ByteBuffer& buffer() { return buf; }

	/**
	 * @brief Finalises the packet by computing and patching
	 * @c payloadLength into the previously written header.
	 *
	 * Must be called exactly once, after all payload bytes have been
	 * written. Subsequent calls are no-ops (safe but wasteful).
	 *
	 * @return The number of payload bytes written (i.e. the patched
	 *         @c payloadLength value).
	 */
	Uint32 finish() {
		if (finished)
			return cachedPayloadLength;

		static constexpr size_t PAYLOAD_LENGTH_OFFSET = 2;

		const Uint32 payloadBytes =
			static_cast<Uint32>(buf.size() - payloadStart);

		buf.patchU32(headerOffset + PAYLOAD_LENGTH_OFFSET, payloadBytes);

		cachedPayloadLength = payloadBytes;
		finished = true;

		return cachedPayloadLength;
	}

	/**
	 * @brief Returns the byte offset within the buffer at which the
	 * header was written.
	 *
	 * Useful when embedding multiple packets in a single buffer and
	 * needing to track individual header positions.
	 */
	size_t headerStartOffset() const { return headerOffset; }

	/**
	 * @brief Returns the total packet size (header + payload) in bytes.
	 *
	 * Only valid after @c finish() has been called.
	 */
	size_t totalSize() const {
		assert(finished && "totalSize() called before finish()");
		return PacketHeader::SERIALIZED_SIZE + cachedPayloadLength;
	}

private:
	Core::ByteBuffer& buf;
	bool finished = false;
	size_t headerOffset = 0; ///< Offset of first header byte in buf.
	size_t payloadStart = 0; ///< Offset of first payload byte in buf.
	Uint32 cachedPayloadLength = 0;
};

} // namespace Blackthorn::Net::Protocol