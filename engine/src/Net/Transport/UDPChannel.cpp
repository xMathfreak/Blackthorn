#include "Net/Transport/UDPChannel.h"

#include "Debug/Logger.h"
#include "Net/PacketHeader.h"

namespace Blackthorn::Net::Transport {

SocketResult UDPChannel::send(
	ISocket& socket,
	const Address& address,
	const Net::ByteBuffer& payload
) {
	Net::ByteBuffer datagram;
	datagram.reserve(UDPHeader::SERIALIZED_SIZE + payload.size());

	UDPHeader hdr;
	hdr.seq = localSeq;
	hdr.ack = remoteSeq;
	hdr.ackBits = ackBits;
	hdr.serialize(datagram);

	datagram.writeBytes(payload.data(), payload.size());

	bool reliable = false;
	if (payload.size() >= Net::PacketHeader::SERIALIZED_SIZE) {
		Net::ByteBuffer tmp(payload.data(), payload.size());

		PacketHeader packetHdr;
		packetHdr.deserialize(tmp);

		reliable = (packetHdr.flags & PacketFlags::Reliable) == PacketFlags::Reliable;
	}

	if (reliable)
		enqueueRetransmit(localSeq, datagram);

	++localSeq;

	if (datagram.size() > PRACTICAL_MTU) {
		BT_WARN(
			"UDPChannel: datagram size {} exceeds practical MTU {} — "
			"may be dropped on internet paths. Consider reducing payload "
			"size or implementing fragmentation.",
			datagram.size(), PRACTICAL_MTU
		);
	}

	return socket.sendTo(datagram.data(), datagram.size(), address);
}

void UDPChannel::processInboundHeader(const UDPHeader& header) {
	if (seqGreaterThan(header.seq, remoteSeq)) {
		Uint16 diff = seqDiff(header.seq, remoteSeq);

		if (diff == 0) {
			// Same packet
		} else if (diff < 32) {
			ackBits = (ackBits << diff) | (1u << (diff - 1));
		} else {
			ackBits = 0;
		}

		remoteSeq = header.seq;
	} else if (header.seq != remoteSeq) {
		Uint16 diff = seqDiff(remoteSeq, header.seq);

		if (diff > 0 && diff <= 32)
			ackBits |= (1u << (diff - 1));
	}

	acknowledgeSeq(header.ack);

	for (int i = 0; i < 32; ++i) {
		if (header.ackBits & (1u << i)) {
			Uint16 ackedSeq = static_cast<Uint16>(header.ack - 1 - i);
			acknowledgeSeq(ackedSeq);
		}
	}
}

void UDPChannel::retransmitPending(ISocket& socket, const Address& address) {
	const Uint64 now = SDL_GetTicks();

	for (auto& entry : retransmitQueue) {
		if (!entry.occupied || entry.acknowledged)
			continue;

		if (now - entry.sentAtMs >= RETRANSMIT_TIMEOUT_MS) {
			BT_DEBUG("UDPChannel: retransmitting seq {}", entry.seq);

			socket.sendTo(entry.payload.data(), entry.payload.size(), address);
			entry.sentAtMs = now;
		}
	}
}

void UDPChannel::enqueueRetransmit(Uint16 seq, const Net::ByteBuffer& payload) {
	for (size_t i = 0; i < MAX_RETRANSMIT_ENTRIES; ++i) {
		size_t idx = (retransmitHead + i) % MAX_RETRANSMIT_ENTRIES;
		auto& entry = retransmitQueue[idx];

		if (!entry.occupied || entry.acknowledged) {
			entry.payload.clear();
			entry.payload.writeBytes(payload.data(), payload.size());
			entry.sentAtMs = SDL_GetTicks();
			entry.seq = seq;
			entry.occupied = true;
			entry.acknowledged = false;

			retransmitHead = (idx + 1) % MAX_RETRANSMIT_ENTRIES;
			return;
		}
	}

	BT_WARN("UDPChannel: retransmit queue full — reliable packet seq {} dropped", seq);
}

void UDPChannel::acknowledgeSeq(Uint16 seq) {
	for (auto& entry : retransmitQueue) {
		if (entry.occupied && !entry.acknowledged && entry.seq == seq) {
			entry.acknowledged = true;
			return;
		}
	}
}

} // namespace Blackthorn::Net::Transport