#include "Net/PacketDispatcher.h"

#include "Debug/Logger.h"
#include "Jobs/JobSystem.h"
#include "Net/Connection/PeerRegistry.h"
#include "Net/Protocol/PacketHeader.h"

namespace Blackthorn::Net {

void PacketDispatcher::poll(Jobs::JobSystem* jobs) {
	if (jobs && pendingJobHandle) {
		jobs->wait(pendingJobHandle);
		pendingJobHandle = nullptr;
	}

	if (++timeoutCheckCounter >= 60) {
		timeoutCheckCounter = 0;
		checkTimeouts();
	}

	dispatchEvents();

	if (!packetHandler)
		return;

	Jobs::JobHandlePtr tickHandle = jobs ? jobs->createHandle() : nullptr;

	Transport::InboundPacket packet;
	while (inboundQueue.pop(packet)) {
		Protocol::PacketHeader header;
		header.deserialize(packet.data);

		if (!header.isValid()) {
			BT_WARN(
				"PacketDispatcher: Dropped packet from peer {}, bad magic",
				packet.peerID
			);

			continue;
		}

		{
			std::lock_guard<std::mutex> lock(registry->mutex());
			const auto* peer = [&]() -> const Connection::NetworkPeer* {
				const auto& list = registry->peerList();
				if (packet.peerID >= list.size())
					return nullptr;

				return &list[packet.peerID];
			}();

			if (!peer || peer->negotiatedSchemaVersion != Protocol::CURRENT_SCHEMA_VERSION) {
				BT_WARN(
					"PacketDispatcher: Dropped packet from peer {}, "
					"schema version mismatch (peer v{}, local v{})",
					packet.peerID,
					peer ? peer->negotiatedSchemaVersion : 0,
					Protocol::CURRENT_SCHEMA_VERSION
				);

				continue;
			}
		}

		const U32 actualBytes = static_cast<U32>(packet.data.remaining());
		if (header.payloadLength != actualBytes) {
			BT_WARN(
				"PacketDispatcher: Dropped packet from peer {}, "
				"payloadLength {} != actual {} bytes",
				packet.peerID, header.payloadLength, actualBytes
			);

			continue;
		}

		Connection::PeerID pid = packet.peerID;
		Protocol::PacketHeader hdr = header;
		IO::ByteBuffer payload = std::move(packet.data);
		auto handler = packetHandler;

		if (jobs) {
			tickHandle->addPending(1);
			jobs->submit(Jobs::Job(
				[pid, hdr, payload = std::move(payload), handler]() mutable {
					handler(pid, hdr, payload);
				},
				tickHandle
			));
		} else {
			handler(pid, hdr, payload);
		}
	}

	if (tickHandle) {
		tickHandle->signal([jobs](std::function<void()> fn, bool isMt) {
			jobs->submit(Jobs::Job(
				std::move(fn), nullptr, nullptr,
				isMt ? Jobs::ThreadAffinity::MainThread
					 : Jobs::ThreadAffinity::Any
			));
		});
	}

	pendingJobHandle = tickHandle;
}

void PacketDispatcher::checkTimeouts() {
	std::vector<Connection::PeerID> timedOut;

	{
		std::lock_guard<std::mutex> lock(registry->mutex());

		for (auto& peer : registry->peerList()) {
			if (!peer.isConnected() || !peer.isTimedOut())
				continue;

			BT_WARN("PacketDispatcher: Peer {} timed out", peer.id);
			timedOut.push_back(peer.id);

			if (peer.tcpSocket)
				peer.tcpSocket->close();

			peer.state = Connection::PeerState::Disconnected;
			peer.tcpConnected = false;
			peer.udpConnected = false;
			registry->tcpMap().erase(peer.tcpAddress);
			registry->udpMap().erase(peer.udpAddress);
		}
	}

	for (Connection::PeerID id : timedOut)
		eventBus->push({ ConnectionEventType::Disconnect, id, {} });
}

void PacketDispatcher::dispatchEvents() {
	eventBus->drainInto(eventScratch);

	for (const auto& ev : eventScratch) {
		switch (ev.type) {
			case ConnectionEventType::Connect:
				if (connectHandler)
					connectHandler(ev.peerID, ev.address);

				break;

			case ConnectionEventType::Disconnect:
				if (disconnectHandler)
					disconnectHandler(ev.peerID);

				break;
		}
	}
}

} // namespace Blackthorn::Net