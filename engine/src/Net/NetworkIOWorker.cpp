#include "Net/NetworkIOWorker.h"

#ifdef _WIN32
	#include <winsock2.h>
#else
	#include <sys/select.h>
#endif

#include "Debug/Logger.h"
#include "Net/Connection/PeerRegistry.h"
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

	BT_LOG("NetworkIOWorker: Stopped");
}

void NetworkIOWorker::ioThreadLoop() {
	Threads::ThreadRegistry::instance().registerCurrent("Net-IO");
	BT_LOG("NetworkIOWorker: I/O thread started");

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
			}
		}

		SDL_DelayNS(static_cast<Uint64>(cfg.pollIntervalMicros) * 1000ULL);
	}

	Threads::ThreadRegistry::instance().unregisterCurrent();
	BT_LOG("NetworkIOWorker: I/O thread stopped");
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

		Core::ByteBuffer datagram(recvScratch.data(), bytesRead);

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

		Connection::PeerId peerId = Connection::INVALID_PEER_ID;
		Connection::PeerId kickedId = Connection::INVALID_PEER_ID;
		bool newUDPPeer = false;
		bool rateDropped = false;

		{
			std::lock_guard<std::mutex> lock(registry->mutex());
			peerId = registry->findOrCreate(
				srcAddress, false, cfg.allowUDPImplicitPeers);

			if (peerId == Connection::INVALID_PEER_ID)
				continue;

			auto& peer = registry->peerList()[peerId];

			if (peer.state == Connection::PeerState::Connecting
				&& !peer.tcpSocket)
			{
				peer.state = Connection::PeerState::Connected;
				peer.negotiatedSchemaVersion = Protocol::CURRENT_SCHEMA_VERSION;
				newUDPPeer = true;
				BT_LOG("NetworkIOWorker: UDP peer {} connected from {}",
					peerId, srcAddress.toString());
			}

			const Connection::RateLimitStage rl =
				peer.rateLimiter.update(bytesRead);

			switch (rl) {
				case Connection::RateLimitStage::Disconnect:
					BT_WARN(
						"NetworkIOWorker: Peer {} force-disconnected (UDP rate "
						"abuse) — peak {:.0f} pkts/s, {:.0f} KB/s, "
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

					kickedId = peerId;
					newUDPPeer = false;
					break;

				case Connection::RateLimitStage::Warn:
					if (peer.rateLimiter.shouldWarn())
						BT_WARN(
							"NetworkIOWorker: Peer {} UDP rate limit — "
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
					break;
			}
		}

		if (newUDPPeer)
			eventBus->push({ ConnectionEventType::Connect, peerId, srcAddress });

		if (kickedId != Connection::INVALID_PEER_ID) {
			eventBus->push({ ConnectionEventType::Disconnect, kickedId, {} });
			continue;
		}

		if (rateDropped)
			continue;

		Core::ByteBuffer payload(
			datagram.data() + datagram.readPosition(),
			datagram.remaining()
		);

		Transport::InboundPacket pkt;
		pkt.source = srcAddress;
		pkt.data = std::move(payload);
		pkt.channel = Transport::InboundPacket::Channel::UDP;
		pkt.peerId = peerId;

		if (!inboundQueue->push(std::move(pkt)))
			BT_WARN("NetworkIOWorker: Inbound queue full — UDP packet dropped");
	}
}

void NetworkIOWorker::pollTCPAccept() {
	if (!tcpListenSocket)
		return;

	Transport::Address clientAddr;
	auto clientSocket = tcpListenSocket->accept(clientAddr);
	if (!clientSocket)
		return;

	Connection::PeerId peerId = Connection::INVALID_PEER_ID;

	{
		std::lock_guard<std::mutex> lock(registry->mutex());
		peerId = registry->allocateSlot(clientAddr, true);

		if (peerId == Connection::INVALID_PEER_ID) {
			BT_WARN(
				"NetworkIOWorker: TCP connection from {} rejected — no free slots",
				clientAddr.toString()
			);

			clientSocket->close();
			return;
		}

		auto& peer = registry->peerList()[peerId];
		peer.tcpSocket = std::move(clientSocket);
		peer.tcpChannel = std::make_unique<Transport::Channels::TCPChannel>();
		peer.markAlive();
	}

	BT_LOG("NetworkIOWorker: TCP accepted from {} (peerId {})",
		clientAddr.toString(), peerId);
}

void NetworkIOWorker::pollTCP() {
	struct DeferredEvent {
		ConnectionEventType type;
		Connection::PeerId peerId;
		Transport::Address address;
	};

	std::vector<DeferredEvent> deferred;

	{
		std::lock_guard<std::mutex> lock(registry->mutex());
		auto& peers = registry->peerList();

		for (auto& peer : peers) {
			if (peer.state == Connection::PeerState::Disconnected)
				continue;

			if (!peer.tcpSocket || !peer.tcpChannel)
				continue;

			if (peer.state == Connection::PeerState::Connecting
				&& !peer.sentConnectRequest
				&& peer.tcpSocket->isConnected())
			{
				Core::ByteBuffer reqBuf;
				Protocol::PacketHeader reqHdr;
				reqHdr.packetType = Protocol::PacketType::ConnectRequest;
				reqHdr.payloadLength = sizeof(Uint16);
				reqHdr.serialize(reqBuf);
				reqBuf.writeU16(Protocol::CURRENT_SCHEMA_VERSION);
				peer.tcpChannel->send(*peer.tcpSocket, reqBuf);
				peer.sentConnectRequest = true;

				BT_LOG(
					"NetworkIOWorker: Sent ConnectRequest (schema v{}) to peer {}",
					Protocol::CURRENT_SCHEMA_VERSION, peer.id
				);
			}

			if (peer.state == Connection::PeerState::Connecting
				&& !peer.tcpSocket->isConnected()
			) {
				continue;
			}

			Core::ByteBuffer msg;
			for (;;) {
				const Transport::Channels::ReceiveResult rr =
					peer.tcpChannel->receive(*peer.tcpSocket, msg);

				if (rr == Transport::Channels::ReceiveResult::FatalError) {
					BT_WARN(
						"NetworkIOWorker: Peer {} TCP framing error — disconnecting",
						peer.id
					);

					peer.tcpSocket->close();
					peer.state = Connection::PeerState::Disconnected;
					peer.tcpConnected = false;
					peer.udpConnected = false;
					registry->tcpMap().erase(peer.tcpAddress);
					registry->udpMap().erase(peer.udpAddress);

					deferred.push_back({
						ConnectionEventType::Disconnect, peer.id, {}
					});

					break;
				}

				if (rr == Transport::Channels::ReceiveResult::NeedMore)
					break;

				peer.markAlive();

				Protocol::PacketHeader header;
				header.deserialize(msg);

				switch (header.packetType) {

					case Protocol::PacketType::ConnectRequest: {
						if (peer.state == Connection::PeerState::Connecting) {
							const Uint16 clientVersion = msg.readU16();

							if (clientVersion != Protocol::CURRENT_SCHEMA_VERSION) {
								BT_WARN(
									"NetworkIOWorker: Peer {} schema mismatch "
									"(client v{}, server v{}) — disconnecting",
									peer.id, clientVersion,
									Protocol::CURRENT_SCHEMA_VERSION
								);

								peer.tcpSocket->close();
								peer.state = Connection::PeerState::Disconnected;
								peer.tcpConnected = false;
								peer.udpConnected = false;
								registry->tcpMap().erase(peer.tcpAddress);
								registry->udpMap().erase(peer.udpAddress);

								deferred.push_back({
									ConnectionEventType::Disconnect, peer.id, {}
								});

								break;
							}

							Core::ByteBuffer ackBuf;
							Protocol::PacketHeader ackHdr;
							ackHdr.packetType = Protocol::PacketType::ConnectAck;
							ackHdr.payloadLength = sizeof(Uint16);
							ackHdr.serialize(ackBuf);
							ackBuf.writeU16(Protocol::CURRENT_SCHEMA_VERSION);
							peer.tcpChannel->send(*peer.tcpSocket, ackBuf);

							peer.state = Connection::PeerState::Connected;
							peer.tcpConnected = true;
							peer.negotiatedSchemaVersion = clientVersion;

							Core::ByteBuffer portBuf;
							Protocol::PacketHeader portHdr;
							portHdr.packetType = Protocol::PacketType::UDPPortInfo;
							portHdr.payloadLength = sizeof(Uint16);
							portHdr.serialize(portBuf);
							portBuf.writeU16(udpSocket->getLocalAddress().port());
							peer.tcpChannel->send(*peer.tcpSocket, portBuf);

							BT_LOG(
								"NetworkIOWorker: Peer {} handshake complete (server, schema v{})",
								peer.id, clientVersion
							);

							deferred.push_back({
								ConnectionEventType::Connect,
								peer.id,
								peer.tcpAddress
							});
						}

						break;
					}

					case Protocol::PacketType::ConnectAck: {
						if (peer.state == Connection::PeerState::Connecting) {
							const Uint16 acceptedVersion = msg.readU16();
							peer.negotiatedSchemaVersion = acceptedVersion;
							peer.state = Connection::PeerState::Connected;
							peer.tcpConnected = true;

							Core::ByteBuffer portBuf;
							Protocol::PacketHeader portHdr;
							portHdr.packetType = Protocol::PacketType::UDPPortInfo;
							portHdr.payloadLength = sizeof(Uint16);
							portHdr.serialize(portBuf);
							portBuf.writeU16(udpSocket->getLocalAddress().port());
							peer.tcpChannel->send(*peer.tcpSocket, portBuf);

							BT_LOG(
								"NetworkIOWorker: Peer {} handshake complete (client, schema v{})",
								peer.id, acceptedVersion
							);

							deferred.push_back({
								ConnectionEventType::Connect,
								peer.id,
								peer.tcpAddress
							});
						}

						break;
					}

					case Protocol::PacketType::UDPPortInfo: {
						const Uint16 remoteUDPPort = msg.readU16();
						const Transport::Address remoteUDP =
							Transport::Address::fromIPv4(
								peer.tcpAddress.ip(), remoteUDPPort);

						if (peer.udpConnected)
							registry->udpMap().erase(peer.udpAddress);

						peer.udpAddress = remoteUDP;
						peer.udpConnected = true;
						registry->udpMap()[remoteUDP] = peer.id;

						BT_LOG("NetworkIOWorker: Peer {} UDP registered as {}",
							peer.id, remoteUDP.toString());

						break;
					}

					case Protocol::PacketType::Heartbeat: {
						Core::ByteBuffer buf;
						Protocol::PacketHeader hdr;
						hdr.packetType = Protocol::PacketType::HeartbeatAck;
						hdr.serialize(buf);
						peer.tcpChannel->send(*peer.tcpSocket, buf);
						BT_LOG("NetworkIOWorker: Heartbeat from peer {}", peer.id);
						break;
					}

					case Protocol::PacketType::HeartbeatAck:
						BT_LOG("NetworkIOWorker: HeartbeatAck from peer {}", peer.id);
						break;

					default: {
						const Connection::RateLimitStage rl =
							peer.rateLimiter.update(msg.size());

						if (rl == Connection::RateLimitStage::Disconnect) {
							BT_WARN(
								"NetworkIOWorker: Peer {} force-disconnected "
								"(TCP rate abuse) — peak {:.0f} pkts/s, "
								"{:.0f} KB/s, sustained {}ms",
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

							deferred.push_back({
								ConnectionEventType::Disconnect, peer.id, {}
							});

							break;
						}

						if (rl == Connection::RateLimitStage::Warn) {
							if (peer.rateLimiter.shouldWarn())
								BT_WARN(
									"NetworkIOWorker: Peer {} TCP rate limit — "
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
						pkt.data = Core::ByteBuffer(msg.data(), msg.size());
						pkt.channel = Transport::InboundPacket::Channel::TCP;
						pkt.peerId = peer.id;

						if (!inboundQueue->push(std::move(pkt)))
							BT_WARN("NetworkIOWorker: Inbound queue full — "
								"TCP packet dropped");

						break;
					}
				}
			}
		}
	}

	for (const auto& d : deferred)
		eventBus->push({ d.type, d.peerId, d.address });
}

void NetworkIOWorker::sendHeartbeats() {
	if (cfg.heartbeatIntervalMs == 0)
		return;

	std::lock_guard<std::mutex> lock(registry->mutex());

	for (auto& peer : registry->peerList()) {
		if (!peer.needsHeartbeat(cfg.heartbeatIntervalMs))
			continue;

		Core::ByteBuffer buf;
		Protocol::PacketHeader hdr;
		hdr.packetType = Protocol::PacketType::Heartbeat;
		hdr.serialize(buf);

		peer.tcpChannel->send(*peer.tcpSocket, buf);
		peer.lastHeartbeatSentMs = SDL_GetTicks();

		BT_DEBUG("NetworkIOWorker: Sent Heartbeat to peer {}", peer.id);
	}
}

} // namespace Blackthorn::Net