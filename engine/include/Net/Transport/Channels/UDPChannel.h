#pragma once

#include <array>

#include <SDL3/SDL.h>

#include "Core/Export.h"
#include "Net/Core/ByteBuffer.h"
#include "Net/Transport/Address.h"
#include "Net/Transport/Sockets/ISocket.h"
#include "Net/Protocol/PacketHeader.h"

namespace Blackthorn::Net::Transport::Channels {

/**
 * @brief Wire header prepended to every UDP datagram, before the PacketHeader.
 *
 * @details Wire layout (8 bytes, little-endian):
 *
 * @code
 * uint16 seq     — outbound sequence number of this packet
 * uint16 ack     — last inbound sequence number received from the remote peer
 * uint32 ackBits — bitmask: bit i set means (ack - 1 - i) was also received
 * @endcode
 *
 * Total UDP overhead per datagram:
 * 8 bytes (@c UDPHeader) + 12 bytes (@c PacketHeader) = 20 bytes.
 */
struct BLACKTHORN_API UDPHeader {
	static constexpr size_t SERIALIZED_SIZE = 8;

	Uint16 seq = 0;
	Uint16 ack = 0;
	Uint32 ackBits = 0;

	void serialize(Core::ByteBuffer& buf) const {
		buf.writeU16(seq);
		buf.writeU16(ack);
		buf.writeU32(ackBits);
	}

	void deserialize(Core::ByteBuffer& buf) {
		seq = buf.readU16();
		ack = buf.readU16();
		ackBits = buf.readU32();
	}
};

static_assert(
	(2 * sizeof(Uint16) + sizeof(Uint32)) == UDPHeader::SERIALIZED_SIZE,
	"UDPHeader wire field sizes do not sum to SERIALIZED_SIZE"
);

/**
 * @brief Per-peer UDP state machine: sequence numbers, ACK bitfield, and
 * reliable packet retransmission.
 *
 * @section Sequence numbers
 *
 * Both the outbound (@c localSeq) and inbound (@c remoteSeq) sequence numbers
 * are 16-bit and wrap freely. Comparisons use sequence arithmetic
 * (@c seqGreaterThan) to handle wrap-around correctly.
 *
 * @section ACK bitfield
 *
 * Every datagram carries the sender's @c remoteSeq (last received sequence)
 * and a 32-bit @c ackBits bitmask where bit i=0 is (remoteSeq-1), bit i=1
 * is (remoteSeq-2), etc. This encodes ACKs for the 32 packets before the
 * latest received without additional overhead.
 *
 * @section Reliability
 *
 * Packets sent with @c PacketFlags::Reliable are copied into the retransmit
 * queue alongside a send timestamp. When an ACK for a packet is received
 * (via the remote's @c ackBits), its entry is marked acknowledged and freed.
 * Unacknowledged entries older than @c retransmitTimeoutMs are resent.
 *
 * Unreliable packets bypass the queue entirely.
 */
class BLACKTHORN_API UDPChannel {
public:
	static constexpr size_t MAX_RETRANSMIT_ENTRIES = 64;
	static constexpr Uint32 RETRANSMIT_TIMEOUT_MS = 100;

	UDPChannel() = default;

	UDPChannel(const UDPChannel&) = delete;
	UDPChannel& operator=(const UDPChannel&) = delete;

	UDPChannel(UDPChannel&&) = default;
	UDPChannel& operator=(UDPChannel&&) = default;

	/// Practical MTU for outbound UDP datagrams, in bytes.
	static constexpr size_t PRACTICAL_MTU = 1400;

	/**
	 * @brief Minimum valid inbound datagram size, in bytes.
	 *
	 * Every datagram must carry at least a @c UDPHeader (8 bytes) and a
	 * @c PacketHeader (12 bytes) = 20 bytes minimum. Anything smaller cannot
	 * be a valid packet and is dropped by @c NetworkIOWorker::pollUDP()
	 * before peer lookup.
	 */
	static constexpr size_t MIN_DATAGRAM_SIZE =
		UDPHeader::SERIALIZED_SIZE + Protocol::PacketHeader::SERIALIZED_SIZE;

	/**
	 * @brief Sends `payload` to `address` via `socket`, prepending a
	 * `UDPHeader` and stamping the next outbound sequence number.
	 *
	 * If `PacketFlags::Reliable` is set in the embedded `PacketHeader`,
	 * a copy is added to the retransmit queue.
	 *
	 * @param socket  Open UDP socket to send through.
	 * @param address Destination peer address.
	 * @param payload A `ByteBuffer` whose content begins at position 0
	 *                and already contains a serialized `PacketHeader`
	 *                followed by the payload bytes.
	 * @return SocketResult of the underlying sendTo call.
	 */
	Sockets::SocketResult send(
		Sockets::ISocket& socket,
		const Address& address,
		const Core::ByteBuffer& payload);

	/**
	 * @brief Processes the `UDPHeader` from a received datagram.
	 *
	 * Updates the remote sequence tracker, marks retransmit queue entries
	 * as acknowledged, and advances the ACK bitmask.
	 *
	 * @param header The deserialized UDPHeader from the incoming datagram.
	 */
	void processInboundHeader(const UDPHeader& header);

	/**
	 * @brief Retransmits any reliable packets that have not been
	 * acknowledged within `RETRANSMIT_TIMEOUT_MS` milliseconds.
	 *
	 * Call once per tick, before dispatching new outbound packets.
	 *
	 * @param socket  Open UDP socket.
	 * @param address Destination peer address.
	 */
	void retransmitPending(Sockets::ISocket& socket, const Address& address);

	Uint16 getLocalSeq() const noexcept { return localSeq; }
	Uint16 getRemoteSeq() const noexcept { return remoteSeq; }
	Uint32 getAckBits() const noexcept { return ackBits; }

	size_t getPendingRetransmitCount() const noexcept {
		size_t count = 0;

		for (const auto& e : retransmitQueue)
			if (e.occupied && !e.acknowledged)
				++count;

		return count;
	}

private:
	static bool seqGreaterThan(Uint16 a, Uint16 b) noexcept {
		constexpr Uint16 val = 0x8000u;
		return ((a > b) && (a - b <= val))
			|| ((a < b) && (b - a > val));
	}

	static Uint16 seqDiff(Uint16 newer, Uint16 older) noexcept {
		return static_cast<Uint16>(newer - older);
	}

	Uint16 localSeq = 0; ///< Next outbound sequence number.
	Uint16 remoteSeq = 0; ///< Latest inbound sequence number received.
	Uint32 ackBits = 0; ///< ACK bitmask for the 32 packets before remoteSeq.

	struct RetransmitEntry {
		Core::ByteBuffer payload; ///< Full datagram bytes (UDPHeader + PacketHeader + data).
		Uint64 sentAtMs = 0; ///< SDL_GetTicks() at time of send.
		Uint16 seq = 0; ///< Sequence number of this packet.
		bool occupied = false;
		bool acknowledged = false;
	};

	std::array<RetransmitEntry, MAX_RETRANSMIT_ENTRIES> retransmitQueue{};
	size_t retransmitHead = 0; ///< Next slot to write into (circular).

	void enqueueRetransmit(Uint16 seq, const Core::ByteBuffer& payload);
	void acknowledgeSeq(Uint16 seq);
};

} // namespace Blackthorn::Net::Transport::Channels