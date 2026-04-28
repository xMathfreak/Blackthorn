#include "Net/Transport/Channels/UDPChannel.h"

#include "Debug/Logger.h"
#include "Net/Protocol/PacketHeader.h"

namespace Blackthorn::Net::Transport::Channels {

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfree-nonheap-object"
#endif

Sockets::SocketResult UDPChannel::send(
	Sockets::ISocket& socket,
	const Address& address,
	const IO::ByteBuffer& payload
) {
	const size_t unfragmentedSize =
		UDPHeader::SERIALIZED_SIZE
		+ Protocol::FragmentHeader::UNFRAGMENTED_SIZE
		+ payload.size();

	if (unfragmentedSize <= PRACTICAL_MTU) {
		IO::ByteBuffer datagram;
		datagram.reserve(unfragmentedSize);

		// UDPHeader
		UDPHeader udpHdr;
		udpHdr.seq = localSeq;
		udpHdr.ack = remoteSeq;
		udpHdr.ackBits = ackBits;
		udpHdr.serialize(datagram);

		// FragmentHeader (not fragmented)
		datagram.writeU8(0);

		datagram.writeBytes(payload.data(), payload.size());

		bool reliable = false;
		if (payload.size() >= Protocol::PacketHeader::SERIALIZED_SIZE) {
			IO::ByteBuffer tmp(payload.data(), payload.size());
			Protocol::PacketHeader ph;
			ph.deserialize(tmp);
			reliable = hasFlag(ph.flags, Protocol::PacketFlags::Reliable);
		}

		return sendDatagram(socket, address, datagram, reliable);
	}

	bool reliable = false;
	if (payload.size() >= Protocol::PacketHeader::SERIALIZED_SIZE) {
		IO::ByteBuffer tmp(payload.data(), payload.size());
		Protocol::PacketHeader ph;
		ph.deserialize(tmp);
		reliable = hasFlag(ph.flags, Protocol::PacketFlags::Reliable);
	}

	const size_t packetHeaderSize = Protocol::PacketHeader::SERIALIZED_SIZE;
	const Uint8* pktHdrBytes = payload.data();
	const Uint8* appBytes = payload.data() + packetHeaderSize;
	const size_t appSize = payload.size() - packetHeaderSize;

	const size_t firstChunk = (appSize <= FRAG_0_PAYLOAD_BYTES)
		? appSize
		: FRAG_0_PAYLOAD_BYTES;

	const size_t remaining = appSize - firstChunk;
	const size_t extraFrags = (remaining + FRAG_N_PAYLOAD_BYTES - 1)
		/ FRAG_N_PAYLOAD_BYTES;
	const size_t totalFrags = 1 + extraFrags;

	if (totalFrags > Protocol::FragmentHeader::MAX_FRAGMENTS) {
		BT_WARN(
			"UDPChannel: payload of {} bytes requires {} fragments "
			"(max {}), dropping",
			payload.size(), totalFrags, Protocol::FragmentHeader::MAX_FRAGMENTS
		);

		return Sockets::SocketResult::Error;
	}

	const Uint16 msgId = nextFragmentId++;

	size_t appOffset = 0;

	for (size_t fragIdx = 0; fragIdx < totalFrags; ++fragIdx) {
		const bool isFirst = (fragIdx == 0);
		const size_t chunkSize = isFirst
			? firstChunk
			: std::min(FRAG_N_PAYLOAD_BYTES, appSize - appOffset);

		IO::ByteBuffer datagram;
		datagram.reserve(PRACTICAL_MTU);

		// UDPHeader
		UDPHeader udpHdr;
		udpHdr.seq = localSeq;
		udpHdr.ack = remoteSeq;
		udpHdr.ackBits = ackBits;
		udpHdr.serialize(datagram);

		// FragmentHeader (5-byte fragmented form)
		Protocol::FragmentHeader fragHdr;
		fragHdr.flags = Protocol::FragmentHeader::FLAG_FRAGMENTED;
		fragHdr.fragmentId = msgId;
		fragHdr.totalFrags = static_cast<Uint8>(totalFrags);
		fragHdr.fragIndex = static_cast<Uint8>(fragIdx);
		fragHdr.serialize(datagram);

		// Fragment 0 (full PacketHeader)
		if (isFirst)
			datagram.writeBytes(pktHdrBytes, packetHeaderSize);

		// Application payload slice.
		datagram.writeBytes(appBytes + appOffset, chunkSize);
		appOffset += chunkSize;

		Sockets::SocketResult result =
			sendDatagram(socket, address, datagram, reliable);

		if (result != Sockets::SocketResult::Ok)
			return result;
	}

	return Sockets::SocketResult::Ok;
}

Sockets::SocketResult UDPChannel::sendDatagram(
	Sockets::ISocket& socket,
	const Address& address,
	const IO::ByteBuffer& datagram,
	bool reliable
) {
	if (reliable)
		enqueueRetransmit(localSeq, datagram);

	++localSeq;

	if (datagram.size() > PRACTICAL_MTU) {
		BT_WARN(
			"UDPChannel: datagram {} bytes exceeds PRACTICAL_MTU {} — "
			"may be dropped on internet paths",
			datagram.size(), PRACTICAL_MTU
		);
	}

	return socket.sendTo(datagram.data(), datagram.size(), address);
}

void UDPChannel::processInboundHeader(const UDPHeader& header) {
	if (seqGreaterThan(header.seq, remoteSeq)) {
		const Uint16 diff = seqDiff(header.seq, remoteSeq);

		if (diff < 32) {
			ackBits = (ackBits << diff) | (1u << (diff - 1));
		} else {
			ackBits = 0;
		}

		remoteSeq = header.seq;
	} else if (header.seq != remoteSeq) {
		const Uint16 diff = seqDiff(remoteSeq, header.seq);

		if (diff > 0 && diff <= 32)
			ackBits |= (1u << (diff - 1));
	}

	acknowledgeSeq(header.ack);

	for (int i = 0; i < 32; ++i) {
		if (header.ackBits & (1u << i)) {
			const Uint16 ackedSeq = static_cast<Uint16>(header.ack - 1 - i);
			acknowledgeSeq(ackedSeq);
		}
	}
}

void UDPChannel::retransmitPending(
	Sockets::ISocket& socket,
	const Address& address
) {
	const Uint64 now = SDL_GetTicks();

	for (auto& entry : retransmitQueue) {
		if (!entry.occupied || entry.acknowledged)
			continue;

		if (now - entry.sentAtMs >= RETRANSMIT_TIMEOUT_MS) {
			BT_DEBUG("UDPChannel: retransmitting seq {}", entry.seq);
			socket.sendTo(
				entry.payload.data(),
				entry.payload.size(),
				address
			);

			entry.sentAtMs = now;
		}
	}
}

void UDPChannel::enqueueRetransmit(
	Uint16 seq,
	const IO::ByteBuffer& datagram
) {
	for (size_t i = 0; i < MAX_RETRANSMIT_ENTRIES; ++i) {
		const size_t idx = (retransmitHead + i) % MAX_RETRANSMIT_ENTRIES;
		auto& entry = retransmitQueue[idx];

		if (!entry.occupied || entry.acknowledged) {
			entry.payload.clear();
			entry.payload.writeBytes(datagram.data(), datagram.size());
			entry.sentAtMs = SDL_GetTicks();
			entry.seq = seq;
			entry.occupied = true;
			entry.acknowledged = false;
			retransmitHead = (idx + 1) % MAX_RETRANSMIT_ENTRIES;

			return;
		}
	}

	BT_WARN(
		"UDPChannel: retransmit queue full — "
		"reliable packet seq {} dropped",
		seq
	);
}

void UDPChannel::acknowledgeSeq(Uint16 seq) {
	for (auto& entry : retransmitQueue) {
		if (entry.occupied && !entry.acknowledged && entry.seq == seq) {
			entry.acknowledged = true;
			return;
		}
	}
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

} // namespace Blackthorn::Net::Transport::Channels