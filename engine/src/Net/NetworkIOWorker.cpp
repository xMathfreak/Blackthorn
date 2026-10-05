#include "Net/NetworkIOWorker.h"

#ifdef _WIN32
	#include <winsock2.h>
#else
	#include <sys/select.h>
#endif

#include "Debug/Logger.h"
#include "Net/Connection/PeerRegistry.h"
#include "Net/Protocol/FragmentAssembler.h"
#include "Net/Protocol/FragmentHeader.h"
#include "Net/Protocol/PacketHeader.h"
#include "Net/Transport/Channels/TCPChannel.h"
#include "Net/Transport/Channels/UDPChannel.h"
#include "Net/Transport/Sockets/SocketFactory.h"
#include "Threads/ThreadRegistry.h"

namespace Blackthorn::Net {

bool NetworkIOWorker::start(
	const ConnectionConfig& config,
	Connection::PeerRegistry& reg,
	ConnectionEventBus& bus,
	Transport::DefaultPacketQueue& queue
) {
	if (ioRunning.load(std::memory_order::relaxed)) {
		BT_WARN("NetworkIOWorker: Already running");
		return false;
	}

	registry = &reg;
	eventBus = &bus;
	inboundQueue = &queue;
	cfg = config;

	recvScratch.resize(RECV_BUFFER_SIZE);

	udpSocket = Transport::Sockets::SocketFactory::createUDP();
	if (!udpSocket) {
		BT_ERROR("NetworkIOWorker: Failed to create UDP socket");
		return false;
	}

	Transport::Address udpBind = Transport::Address::anyIPv4(cfg.udpPort);
	if (!udpSocket->bind(udpBind)) {
		BT_ERROR("NetworkIOWorker: Failed to bind UDP on port {}", cfg.udpPort);
		return false;
	}

	BT_LOG("NetworkIOWorker: UDP bound on port {}",
		udpSocket->getLocalAddress().port());

	if (cfg.tcpPort > 0) {
		tcpListenSocket = Transport::Sockets::SocketFactory::createTCP();
		if (!tcpListenSocket) {
			BT_ERROR("NetworkIOWorker: Failed to create TCP listen socket");
			return false;
		}

		Transport::Address tcpBind = Transport::Address::anyIPv4(cfg.tcpPort);
		if (!tcpListenSocket->bind(tcpBind) || !tcpListenSocket->listen()) {
			BT_ERROR("NetworkIOWorker: Failed to bind/listen TCP on port {}", cfg.tcpPort);
			return false;
		}

		BT_LOG("NetworkIOWorker: TCP listening on port {}", cfg.tcpPort);
	}

	ioRunning.store(true, std::memory_order::release);
	ioThread = std::thread([this] { ioThreadLoop(); });

	BT_LOG("NetworkIOWorker: I/O worker started");
	return true;
}

void NetworkIOWorker::stop() {
	if (!ioRunning.exchange(false, std::memory_order::acq_rel))
		return;

	if (ioThread.joinable())
		ioThread.join();

	if (udpSocket)
		udpSocket->close();

	if (tcpListenSocket)
		tcpListenSocket->close();

	BT_LOG("NetworkIOWorker: I/O worker stopped");
}

void NetworkIOWorker::ioThreadLoop() {
	Threads::ThreadRegistry::instance().registerCurrent("Net-IO");
	BT_DEBUG("NetworkIOWorker: I/O thread started");

	while (ioRunning.load(std::memory_order::relaxed)) {
		pollUDP();

		if (tcpListenSocket)
			pollTCPAccept();

		pollTCP();
		sendHeartbeats();

		{
			std::lock_guard<std::mutex> lock(registry->mutex());
			for (auto& peer : registry->peerList()) {
				if (peer.udpConnected)
					peer.udpChannel.retransmitPending(*udpSocket, peer.udpAddress);

				if (peer.fragmentAssembler)
					peer.fragmentAssembler->evictExpired();
			}
		}

		SDL_DelayNS(static_cast<U64>(cfg.pollIntervalMicros) * 1000ULL);
	}

	Threads::ThreadRegistry::instance().unregisterCurrent();
	BT_DEBUG("NetworkIOWorker: I/O thread stopped");
}

void NetworkIOWorker::pollUDP() {
	if (!udpSocket || !udpSocket->isOpen())
		return;

	for (;;) {
		Transport::Address srcAddress;
		size_t bytesRead = 0;

		Transport::Sockets::SocketResult result = udpSocket->recvFrom(
			recvScratch.data(),
			recvScratch.size(),
			bytesRead,
			srcAddress
		);

		if (result == Transport::Sockets::SocketResult::WouldBlock)
			break;

		if (result != Transport::Sockets::SocketResult::Ok || bytesRead == 0)
			break;

		IO::ByteBuffer datagram(recvScratch.data(), bytesRead);

		if (datagram.remaining() < Transport::Channels::UDPChannel::MIN_DATAGRAM_SIZE) {
			BT_WARN(
				"NetworkIOWorker: Dropped undersized UDP datagram "
				"({} bytes, minimum {})",
				bytesRead, Transport::Channels::UDPChannel::MIN_DATAGRAM_SIZE
			);
			continue;
		}

		Transport::Channels::UDPHeader udpHdr;
		udpHdr.deserialize(datagram);

		if (datagram.remaining() < 1)
			continue;

		Protocol::FragmentHeader fragHdr;
		fragHdr.deserialize(datagram);

		if (fragHdr.isFragmented() && fragHdr.totalFrags == 0) {
			BT_WARN("NetworkIOWorker: Malformed fragment header, dropped");
			continue;
		}

		Connection::PeerID peerID = Connection::INVALID_PEER_ID;
		Connection::PeerID kickedID = Connection::INVALID_PEER_ID;
		bool newUDPPeer = false;
		bool rateDropped = false;

		std::optional<IO::ByteBuffer> reassembled;

		{
			std::lock_guard<std::mutex> lock(registry->mutex());
			peerID = registry->findOrCreate(
				srcAddress, false, cfg.allowUDPImplicitPeers);

			if (peerID == Connection::INVALID_PEER_ID)
				continue;

			auto& peer = registry->peerList()[peerID];

			if (peer.state == Connection::PeerState::Connecting
				&& !peer.tcpSocket)
			{
				peer.state = Connection::PeerState::Connected;
				peer.negotiatedSchemaVersion = Protocol::CURRENT_SCHEMA_VERSION;
				newUDPPeer = true;
				BT_DEBUG("NetworkIOWorker: UDP peer {} connected from {}",
					peerID, srcAddress.toString());
			}

			const Connection::RateLimitStage rl =
				peer.rateLimiter.update(bytesRead);

			switch (rl) {
				case Connection::RateLimitStage::Disconnect:
					BT_WARN(
						"NetworkIOWorker: Peer {} force-disconnected (UDP rate "
						"abuse); peak {:.0f} pkts/s, {:.0f} KB/s, "
						"sustained {}ms",
						peer.id,
						peer.rateLimiter.peakPacketRate,
						peer.rateLimiter.peakByteRate / 1024.0f,
						peer.rateLimiter.stageDurationMs()
					);

					if (peer.tcpSocket)
						peer.tcpSocket->close();

					peer.state = Connection::PeerState::Disconnected;
					peer.tcpConnected = false;
					peer.udpConnected = false;
					registry->tcpMap().erase(peer.tcpAddress);
					registry->udpMap().erase(peer.udpAddress);

					kickedID = peerID;
					newUDPPeer = false;
					break;

				case Connection::RateLimitStage::Warn:
					if (peer.rateLimiter.shouldWarn())
						BT_WARN(
							"NetworkIOWorker: Peer {} UDP rate limit: "
							"{:.0f} pkts/s, {:.0f} KB/s (dropping)",
							peer.id,
							peer.rateLimiter.peakPacketRate,
							peer.rateLimiter.peakByteRate / 1024.0f
						);

					rateDropped = true;
					break;

				case Connection::RateLimitStage::Drop:
					rateDropped = true;
					break;

				default:
					peer.udpChannel.processInboundHeader(udpHdr);
					peer.markAlive();

					if (fragHdr.isFragmented()) {
						if (peer.fragmentAssembler) {
							IO::ByteBuffer slice(
								datagram.data() + datagram.readPosition(),
								datagram.remaining()
							);

							reassembled = peer.fragmentAssembler->ingest(
								fragHdr, slice
							);
						}
					}

					break;
			}
		}

		if (newUDPPeer)
			eventBus->push({ ConnectionEventType::Connect, peerID, srcAddress });

		if (kickedID != Connection::INVALID_PEER_ID) {
			eventBus->push({ ConnectionEventType::Disconnect, kickedID, {} });
			continue;
		}

		if (rateDropped)
			continue;

		if (fragHdr.isFragmented()) {
			if (!reassembled.has_value())
				continue;

			Transport::InboundPacket pkt;
			pkt.source = srcAddress;
			pkt.data = std::move(*reassembled);
			pkt.channel = Transport::InboundPacket::Channel::UDP;
			pkt.peerID = peerID;

			if (!inboundQueue->push(std::move(pkt)))
				BT_WARN(
					"NetworkIOWorker: Inbound queue full, reassembled UDP packet dropped"
				);

			continue;
		}

		IO::ByteBuffer payload(
			datagram.data() + datagram.readPosition(),
			datagram.remaining()
		);

		Transport::InboundPacket pkt;
		pkt.source = srcAddress;
		pkt.data = std::move(payload);
		pkt.channel = Transport::InboundPacket::Channel::UDP;
		pkt.peerID = peerID;

		if (!inboundQueue->push(std::move(pkt)))
			BT_WARN("NetworkIOWorker: Inbound queue full, UDP packet dropped");
	}
}

void NetworkIOWorker::pollTCPAccept() {
	if (!tcpListenSocket)
		return;

	Transport::Address clientAddr;
	auto clientSocket = tcpListenSocket->accept(clientAddr);
	if (!clientSocket)
		return;

	Connection::PeerID peerID = Connection::INVALID_PEER_ID;

	{
		std::lock_guard<std::mutex> lock(registry->mutex());
		peerID = registry->allocateSlot(clientAddr, true);

		if (peerID == Connection::INVALID_PEER_ID) {
			BT_WARN(
				"NetworkIOWorker: TCP connection from {} rejected: no free slots",
				clientAddr.toString()
			);

			clientSocket->close();
			return;
		}

		auto& peer = registry->peerList()[peerID];
		peer.tcpSocket = std::move(clientSocket);
		peer.tcpChannel = std::make_unique<Transport::Channels::TCPChannel>();
		Connection::HandshakeMachine::begin(peer, Connection::PeerOrigin::Inbound);
		peer.markAlive();
	}

	BT_DEBUG(
		"NetworkIOWorker: TCP accepted from {} (peerID {})",
		clientAddr.toString(), peerID
	);
}

void NetworkIOWorker::pollTCP() {
	std::vector<DeferredEvent> deferred;
	std::vector<Connection::HandshakeAction> actions;

	const U64 nowMs = SDL_GetTicks();
	const U16 localUDPPort = udpSocket ? udpSocket->getLocalAddress().port() : 0;

	{
		std::lock_guard<std::mutex> lock(registry->mutex());
		auto& peers = registry->peerList();

		for (auto& peer : peers) {
			if (peer.state == Connection::PeerState::Disconnected)
				continue;

			if (!peer.tcpSocket || !peer.tcpChannel)
				continue;

			if (handshake.isTimedOut(peer, nowMs, cfg.handshakeTimeoutMs)) {
				BT_WARN(
					"NetworkIOWorker: Peer {} handshake timed out after {}ms",
					peer.id, cfg.handshakeTimeoutMs
				);

				closeTCPPeer(peer, deferred);
				continue;
			}

			if (peer.state == Connection::PeerState::Connecting
				&& peer.tcpSocket->isConnected())
			{
				actions.clear();
				handshake.onSocketReady(peer, actions);
				executeHandshakeActions(peer, actions, deferred);
			}

			if (!peer.tcpSocket->isConnected())
				continue;

			IO::ByteBuffer msg;
			for (;;) {
				const Transport::Channels::ReceiveResult rr =
					peer.tcpChannel->receive(*peer.tcpSocket, msg);

				if (rr == Transport::Channels::ReceiveResult::FatalError) {
					BT_WARN(
						"NetworkIOWorker: Peer {} TCP framing error, disconnecting",
						peer.id
					);

					closeTCPPeer(peer, deferred);
					break;
				}

				if (rr == Transport::Channels::ReceiveResult::NeedMore)
					break;

				peer.markAlive();

				Protocol::PacketHeader header;
				header.deserialize(msg);

				actions.clear();
				if (handshake.onPacket(
					peer, header.packetType, msg, localUDPPort, actions))
				{
					executeHandshakeActions(peer, actions, deferred);

					if (peer.state == Connection::PeerState::Disconnected)
						break;

					continue;
				}

				switch (header.packetType) {

					case Protocol::PacketType::UDPPortInfo: {
						const U16 remoteUDPPort = msg.readU16();
						const Transport::Address remoteUDP =
							Transport::Address::fromIPv4(
								peer.tcpAddress.ip(), remoteUDPPort);

						if (peer.udpConnected)
							registry->udpMap().erase(peer.udpAddress);

						peer.udpAddress = remoteUDP;
						peer.udpConnected = true;
						registry->udpMap()[remoteUDP] = peer.id;

						BT_DEBUG("NetworkIOWorker: Peer {} UDP registered as {}",
							peer.id, remoteUDP.toString());

						break;
					}

					case Protocol::PacketType::Heartbeat: {
						IO::ByteBuffer buf;
						Protocol::PacketHeader hdr;
						hdr.packetType = Protocol::PacketType::HeartbeatAck;
						hdr.serialize(buf);
						peer.tcpChannel->send(*peer.tcpSocket, buf);
						BT_TRACE("NetworkIOWorker: Heartbeat from peer {}", peer.id);
						break;
					}

					case Protocol::PacketType::HeartbeatAck:
						BT_TRACE("NetworkIOWorker: HeartbeatAck from peer {}", peer.id);
						break;

					default: {
						const Connection::RateLimitStage rl =
							peer.rateLimiter.update(msg.size());

						if (rl == Connection::RateLimitStage::Disconnect) {
							BT_WARN(
								"NetworkIOWorker: Peer {} force-disconnected "
								"(TCP rate abuse); peak {:.0f} pkts/s, "
								"{:.0f} KB/s, sustained {}ms",
								peer.id,
								peer.rateLimiter.peakPacketRate,
								peer.rateLimiter.peakByteRate / 1024.0f,
								peer.rateLimiter.stageDurationMs()
							);

							closeTCPPeer(peer, deferred);
							break;
						}

						if (rl == Connection::RateLimitStage::Warn) {
							if (peer.rateLimiter.shouldWarn())
								BT_WARN(
									"NetworkIOWorker: Peer {} TCP rate limit: "
									"{:.0f} pkts/s, {:.0f} KB/s (dropping)",
									peer.id,
									peer.rateLimiter.peakPacketRate,
									peer.rateLimiter.peakByteRate / 1024.0f
								);

							break;
						}

						if (rl == Connection::RateLimitStage::Drop)
							break;

						Transport::InboundPacket pkt;
						pkt.source = peer.tcpAddress;
						pkt.data = IO::ByteBuffer(msg.data(), msg.size());
						pkt.channel = Transport::InboundPacket::Channel::TCP;
						pkt.peerID = peer.id;

						if (!inboundQueue->push(std::move(pkt)))
							BT_WARN("NetworkIOWorker: Inbound queue full, "
								"TCP packet dropped");

						break;
					}
				}

				if (peer.state == Connection::PeerState::Disconnected)
					break;
			}
		}
	}

	for (const auto& d : deferred)
		eventBus->push({ d.type, d.peerID, d.address });
}

void NetworkIOWorker::executeHandshakeActions(
	Connection::NetworkPeer& peer,
	const std::vector<Connection::HandshakeAction>& actions,
	std::vector<DeferredEvent>& deferred
) {
	for (const auto& act : actions) {
		switch (act.type) {
			case Connection::HandshakeAction::Type::Send:
				if (peer.tcpSocket && peer.tcpChannel)
					peer.tcpChannel->send(*peer.tcpSocket, act.bytes);
				break;

			case Connection::HandshakeAction::Type::Established:
				peer.state = Connection::PeerState::Connected;
				peer.tcpConnected = true;
				peer.negotiatedSchemaVersion = act.schemaVersion;

				BT_DEBUG(
					"NetworkIOWorker: Peer {} handshake complete ({}, schema v{})",
					peer.id,
					peer.origin == Connection::PeerOrigin::Outbound
						? "client" : "server",
					act.schemaVersion
				);

				deferred.push_back({
					ConnectionEventType::Connect, peer.id, peer.tcpAddress
				});
				break;

			case Connection::HandshakeAction::Type::Reject:
				BT_WARN(
					"NetworkIOWorker: Peer {} handshake rejected: {} "
					"(remote schema v{})",
					peer.id,
					act.reason ? act.reason : "unknown",
					act.schemaVersion
				);

				closeTCPPeer(peer, deferred);
				return;
		}
	}
}

void NetworkIOWorker::closeTCPPeer(
	Connection::NetworkPeer& peer,
	std::vector<DeferredEvent>& deferred
) {
	if (peer.tcpSocket)
		peer.tcpSocket->close();

	peer.state = Connection::PeerState::Disconnected;
	peer.handshakePhase = Connection::HandshakePhase::Failed;
	peer.tcpConnected = false;
	peer.udpConnected = false;
	registry->tcpMap().erase(peer.tcpAddress);
	registry->udpMap().erase(peer.udpAddress);

	deferred.push_back({
		ConnectionEventType::Disconnect, peer.id, {}
	});
}

void NetworkIOWorker::sendHeartbeats() {
	if (cfg.heartbeatIntervalMs == 0)
		return;

	std::lock_guard<std::mutex> lock(registry->mutex());

	for (auto& peer : registry->peerList()) {
		if (!peer.needsHeartbeat(cfg.heartbeatIntervalMs))
			continue;

		IO::ByteBuffer buf;
		Protocol::PacketHeader hdr;
		hdr.packetType = Protocol::PacketType::Heartbeat;
		hdr.serialize(buf);

		peer.tcpChannel->send(*peer.tcpSocket, buf);
		peer.lastHeartbeatSentMs = SDL_GetTicks();

		BT_TRACE("NetworkIOWorker: Sent Heartbeat to peer {}", peer.id);
	}
}

} // namespace Blackthorn::Net