#pragma once

#include <array>

#include "Core/Export.h"
#include "Core/Types/Types.h"
#include "IO/ByteBuffer.h"
#include "Net/Transport/Address.h"
#include "Net/Transport/Sockets/ISocket.h"
#include "Net/Protocol/PacketHeader.h"
#include "Net/Protocol/FragmentHeader.h"

namespace Blackthorn::Net::Transport::Channels {

/**
 * @brief Wire header prepended to every UDP datagram, before the PacketHeader.
 *
 * @details Wire layout (8 bytes, little-endian):
 *
 * @code
 * U16 seq     - outbound sequence number of this packet
 * U16 ack     - last inbound sequence number received from the remote peer
 * U32 ackBits - bitmask: bit i set means (ack - 1 - i) was also received
 * @endcode
 *
 * Total UDP overhead per datagram:
 * 8 bytes (@c UDPHeader) + 12 bytes (@c PacketHeader) = 20 bytes.
 */
struct BLACKTHORN_API UDPHeader {
	static constexpr size_t SERIALIZED_SIZE = 8;

	U16 seq = 0;
	U16 ack = 0;
	U32 ackBits = 0;

	void serialize(IO::ByteBuffer& buf) const {
		buf.writeU16(seq);
		buf.writeU16(ack);
		buf.writeU32(ackBits);
	}

	void deserialize(IO::ByteBuffer& buf) {
		seq = buf.readU16();
		ack = buf.readU16();
		ackBits = buf.readU32();
	}
};

static_assert(
	(2 * sizeof(U16) + sizeof(U32)) == UDPHeader::SERIALIZED_SIZE,
	"UDPHeader wire field sizes do not sum to SERIALIZED_SIZE"
);

/**
 * @brief Per-peer UDP state machine: sequence numbers, ACK bitfield,
 * reliable packet retransmission, and transparent fragmentation.
 *
 * @section Fragmentation
 *
 * When the assembled datagram (UDPHeader + FragmentHeader + PacketHeader +
 * payload) would exceed @c PRACTICAL_MTU, @c send() automatically splits the
 * payload into fragments. Each fragment is a separate datagram carrying the
 * @c UDPHeader (for ACK tracking) and a @c FragmentHeader identifying its
 * position in the sequence. The @c PacketHeader is only present in fragment 0.
 *
 * The usable payload per fragment is:
 *   - Fragment 0:    PRACTICAL_MTU - UDPHeader(8) - FragHeader(5) - PacketHeader(12) = 1375 bytes
 *   - Fragment 1..N: PRACTICAL_MTU - UDPHeader(8) - FragHeader(5)                   = 1387 bytes
 *
 * Reassembly lives in @c FragmentAssembler (one per peer, in @c NetworkPeer).
 *
 * @section Sequence numbers and ACK bitfield
 *
 * See class documentation for ACK bitmask semantics — unchanged from the
 * non-fragmented design.
 *
 * @section Reliability
 *
 * Packets marked @c PacketFlags::Reliable are enqueued for retransmission.
 * Fragmented reliable packets enqueue all fragments individually so the
 * retransmit logic can re-send only the missing ones (once per-fragment ACK
 * tracking is implemented; currently the whole set is retransmitted).
 */

class BLACKTHORN_API UDPChannel {
public:
	static constexpr size_t MAX_RETRANSMIT_ENTRIES = 64;
	static constexpr U32 RETRANSMIT_TIMEOUT_MS = 100;

	UDPChannel() = default;

	UDPChannel(const UDPChannel&) = delete;
	UDPChannel& operator=(const UDPChannel&) = delete;

	UDPChannel(UDPChannel&&) = default;
	UDPChannel& operator=(UDPChannel&&) = default;

	/// Practical MTU for outbound UDP datagrams, in bytes.
	static constexpr size_t PRACTICAL_MTU = 1400;

	/// Per-fragment overhead: UDPHeader(8) + FrgamentHeader fragmented form(5).
	static constexpr size_t FRAGMENT_OVERHEAD =
		UDPHeader::SERIALIZED_SIZE
		+ Protocol::FragmentHeader::FRAGMENTED_SIZE;

	/// Usable payload bytes in fragment 0 (also carries PacketHeader).
	static constexpr size_t FRAG_0_PAYLOAD_BYTES =
		PRACTICAL_MTU
		- FRAGMENT_OVERHEAD
		- Protocol::PacketHeader::SERIALIZED_SIZE;

	/// Usable payload bytes in fragments 1...N.
	static constexpr size_t FRAG_N_PAYLOAD_BYTES =
		PRACTICAL_MTU
		- FRAGMENT_OVERHEAD;

	/**
	 * @brief Minimum valid inbound datagram size, in bytes.
	 *
	 * Every datagram must carry at least a @c UDPHeader (8 bytes) and a
	 * @c FragmentHeader flags byte (1 byte). The @c PacketHeader (12 bytes)
	 * follows only if the packet is unfragmented or is fragment 0.
	 *
	 * The minimum is therefore UDPHeader + FragmentHeader(unfragmented) +
	 * PacketHeader = 8 + 1 + 12 = 21 bytes.
	 */
	static constexpr size_t MIN_DATAGRAM_SIZE =
		UDPHeader::SERIALIZED_SIZE
		+ Protocol::PacketHeader::SERIALIZED_SIZE
		+ Protocol::FragmentHeader::UNFRAGMENTED_SIZE;

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
		const IO::ByteBuffer& payload);

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

	U16 getLocalSeq() const noexcept { return localSeq; }
	U16 getRemoteSeq() const noexcept { return remoteSeq; }
	U32 getAckBits() const noexcept { return ackBits; }

	size_t getPendingRetransmitCount() const noexcept {
		size_t count = 0;

		for (const auto& e : retransmitQueue)
			if (e.occupied && !e.acknowledged)
				++count;

		return count;
	}

private:
	/// Sends a single datagram (either unfragmented or one fragment of many).
	/// Stanps the next @c localSeq and optionally enqueues for retransmit.
	Sockets::SocketResult sendDatagram(
		Sockets::ISocket& socket,
		const Address& address,
		const IO::ByteBuffer& datagram,
		bool reliable
	);

	static bool seqGreaterThan(U16 a, U16 b) noexcept {
		constexpr U16 val = 0x8000u;
		return ((a > b) && (a - b <= val))
			|| ((a < b) && (b - a > val));
	}

	static U16 seqDiff(U16 newer, U16 older) noexcept {
		return static_cast<U16>(newer - older);
	}

	U16 localSeq = 0; ///< Next outbound sequence number.
	U16 remoteSeq = 0; ///< Latest inbound sequence number received.
	U32 ackBits = 0; ///< ACK bitmask for the 32 packets before remoteSeq.

	/// Per-peer fragment message Id counter.
	/// Used by send() to tag all datagrams belonging to the same logical message.
	U16 nextFragmentId = 0;

	struct RetransmitEntry {
		IO::ByteBuffer payload; ///< Full datagram bytes (UDPHeader + PacketHeader + data).
		U64 sentAtMs = 0; ///< SDL_GetTicks() at time of send.
		U16 seq = 0; ///< Sequence number of this packet.
		bool occupied = false;
		bool acknowledged = false;
	};

	std::array<RetransmitEntry, MAX_RETRANSMIT_ENTRIES> retransmitQueue{};
	size_t retransmitHead = 0; ///< Next slot to write into (circular).

	void enqueueRetransmit(U16 seq, const IO::ByteBuffer& payload);
	void acknowledgeSeq(U16 seq);
};

} // namespace Blackthorn::Net::Transport::Channels