#include "Net/ConnectionManager.h"

#ifdef _WIN32
	#include <winsock2.h>
#else
	#include <sys/select.h>
#endif

#include "Debug/Logger.h"
#include "Jobs/JobSystem.h"
#include "Net/Protocol/PacketHeader.h"
#include "Net/Transport/Sockets/SocketFactory.h"
#include "Net/Transport/Sockets/TCPSocket.h"
#include "Threads/ThreadRegistry.h"

namespace Blackthorn::Net {

ConnectionManager::~ConnectionManager() {
	stop();
}

bool ConnectionManager::start(const ConnectionConfig& config) {
	if (ioRunning.load(std::memory_order::relaxed)) {
		BT_WARN("ConnectionManager: Already running");
		return false;
	}

	cfg = config;
	peers.resize(cfg.maxPeers);
	recvScratch.resize(RECV_BUFFER_SIZE);

	udpSocket = Transport::Sockets::SocketFactory::createUDP();
	if (!udpSocket) {
		BT_ERROR("ConnectionManager: Failed to create UDP socket");
		return false;
	}

	Transport::Address udpBind = Transport::Address::anyIPv4(cfg.udpPort);
	if (!udpSocket->bind(udpBind)) {
		BT_ERROR("ConnectionManager: Failed to bind UDP socket on port {}", cfg.udpPort);
		return false;
	}

	if (cfg.tcpPort > 0) {
		tcpListenSocket = Transport::Sockets::SocketFactory::createTCP();
		if (!tcpListenSocket) {
			BT_ERROR("ConnectionManager: Failed to create TCP listen socket");
			return false;
		}

		Transport::Address tcpBind = Transport::Address::anyIPv4(cfg.tcpPort);
		if (!tcpListenSocket->bind(tcpBind) || !tcpListenSocket->listen()) {
			BT_ERROR("ConnectionManager: Failed to bind/listen TCP on port {}", cfg.tcpPort);
			return false;
		}

		BT_LOG("ConnectionManager: TCP listening on port {}", cfg.tcpPort);
	}

	BT_LOG(
		"ConnectionManager: UDP bound on port {}",
		udpSocket->getLocalAddress().port()
	);

	ioRunning.store(true, std::memory_order::release);
	ioThread = std::thread([this] { ioThreadLoop(); });

	return true;
}

void ConnectionManager::stop() {
	if (!ioRunning.exchange(false, std::memory_order::acq_rel))
		return;

	if (ioThread.joinable())
		ioThread.join();

	{
		std::lock_guard<std::mutex> lock(eventMutex);
		pendingEvents.clear();
	}

	pendingJobHandle = nullptr;

	std::lock_guard<std::mutex> lock(peerMutex);

	for (auto& peer : peers) {
		if (peer.tcpSocket)
			peer.tcpSocket->close();

		peer.state = Connection::PeerState::Disconnected;
	}

	if (udpSocket)
		udpSocket->close();

	if (tcpListenSocket)
		tcpListenSocket->close();

	addressToPeerTCP.clear();
	addressToPeerUDP.clear();

	BT_LOG("ConnectionManager: Stopped");
}

Connection::PeerId ConnectionManager::connect(const Transport::Address& address) {
	std::lock_guard<std::mutex> lock(peerMutex);

	Connection::PeerId id = allocatePeerSlot(address, true);
	if (id == Connection::INVALID_PEER_ID) {
		BT_ERROR("ConnectionManager: No free peer slots for {}", address.toString());
		return Connection::INVALID_PEER_ID;
	}

	auto& peer = peers[id];

	if (peer.tcpConnected) {
		peer.markAlive();
		return id;
	}

	auto tcpSock = Transport::Sockets::SocketFactory::createTCP();
	if (tcpSock && tcpSock->connect(address)) {
		peer.tcpSocket = std::move(tcpSock);
		peer.tcpChannel = std::make_unique<Transport::Channels::TCPChannel>();
		peer.state = Connection::PeerState::Connecting;

		BT_LOG(
			"ConnectionManager: TCP connecting to {} (peerId {})",
			address.toString(), id
		);
	} else {
		peer.state = Connection::PeerState::Connecting;
		BT_LOG(
			"ConnectionManager: TCP unavailable for {}; UDP-only (peerId {})",
			address.toString(), id
		);
	}

	peer.markAlive();
	return id;
}

void ConnectionManager::disconnect(Connection::PeerId peerId) {
	std::lock_guard<std::mutex> lock(peerMutex);

	if (peerId >= peers.size())
		return;

	auto& peer = peers[peerId];
	if (peer.state == Connection::PeerState::Disconnected)
		return;

	if (peer.tcpConnected && peer.tcpSocket && peer.tcpChannel) {
		Core::ByteBuffer buf;
		Protocol::PacketHeader hdr;
		hdr.packetType = Protocol::PacketType::Disconnect;
		hdr.tick = 0;
		hdr.payloadLength = 0;
		hdr.serialize(buf);
		peer.tcpChannel->send(*peer.tcpSocket, buf);
	}

	if (peer.tcpSocket)
		peer.tcpSocket->close();

	peer.state = Connection::PeerState::Disconnected;
	peer.tcpConnected = false;
	peer.udpConnected = false;

	addressToPeerTCP.erase(peer.tcpAddress);
	addressToPeerUDP.erase(peer.udpAddress);

	BT_LOG("ConnectionManager: Peer {} disconnected", peerId);
}

bool ConnectionManager::sendUDP(Connection::PeerId peerId, const Core::ByteBuffer& payload) {
	std::lock_guard<std::mutex> lock(peerMutex);

	if (peerId >= peers.size())
		return false;

	auto& peer = peers[peerId];

	if (!peer.udpConnected)
		return false;

	auto result = peer.udpChannel.send(*udpSocket, peer.udpAddress, payload);

	if (result != Transport::Sockets::SocketResult::Ok)
		BT_WARN("ConnectionManager: UDP send failed to {}", peer.udpAddress.toString());

	return result == Transport::Sockets::SocketResult::Ok;
}

bool ConnectionManager::sendTCP(Connection::PeerId peerId, const Core::ByteBuffer& payload) {
	std::lock_guard<std::mutex> lock(peerMutex);

	if (peerId >= peers.size())
		return false;

	auto& peer = peers[peerId];
	if (!peer.isConnected() || !peer.tcpSocket || !peer.tcpChannel)
		return false;

	auto result = peer.tcpChannel->send(*peer.tcpSocket, payload);
	return result == Transport::Sockets::SocketResult::Ok;
}

void ConnectionManager::broadcastUDP(const Core::ByteBuffer& payload) {
	std::lock_guard<std::mutex> lock(peerMutex);

	for (auto& peer : peers) {
		if (peer.udpConnected)
			peer.udpChannel.send(*udpSocket, peer.udpAddress, payload);
	}
}

void ConnectionManager::broadcastTCP(const Core::ByteBuffer& payload) {
	std::lock_guard<std::mutex> lock(peerMutex);

	for (auto& peer : peers) {
		if (peer.isConnected() && peer.tcpSocket && peer.tcpChannel)
			peer.tcpChannel->send(*peer.tcpSocket, payload);
	}
}

void ConnectionManager::poll(Jobs::JobSystem* jobs) {
	if (jobs && pendingJobHandle) {
		jobs->wait(pendingJobHandle);
		pendingJobHandle = nullptr;
	}

	checkTimeouts();
	dispatchPendingEvents();

	if (!packetHandler)
		return;

	Jobs::JobHandlePtr tickHandle = jobs ? jobs->createHandle() : nullptr;

	Transport::InboundPacket packet;
	while (inboundQueue.pop(packet)) {
		Protocol::PacketHeader header;
		header.deserialize(packet.data);

		if (!header.isValid()) {
			BT_WARN(
				"ConnectionManager: Dropped packet from peer {} - bad magic or schema version",
				packet.peerId
			);

			continue;
		}

		const Uint32 actualBytes = static_cast<Uint32>(packet.data.remaining());
		if (header.payloadLength != actualBytes) {
			BT_WARN(
				"ConnectionManager: Dropped packet from peer {} — payloadLength {} != actual bytes {}",
				packet.peerId, header.payloadLength, actualBytes
			);

			continue;
		}

		Connection::PeerId pid = packet.peerId;
		Protocol::PacketHeader hdr = header;
		Core::ByteBuffer payload = std::move(packet.data);
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
				isMt ? Jobs::ThreadAffinity::MainThread : Jobs::ThreadAffinity::Any
			));
		});
	}


	pendingJobHandle = tickHandle;
}

void ConnectionManager::pushEvent(ConnectionEvent event) {
	std::lock_guard<std::mutex> lock(eventMutex);
	pendingEvents.push_back(std::move(event));
}

void ConnectionManager::dispatchPendingEvents() {
	std::vector<ConnectionEvent> toFire;
	{
		std::lock_guard<std::mutex> lock(eventMutex);
		toFire.swap(pendingEvents);
	}

	for (const auto& ev : toFire) {
		switch (ev.type) {
			case ConnectionEventType::Connect:
				if (connectHandler)
					connectHandler(ev.peerId, ev.address);
				break;

			case ConnectionEventType::Disconnect:
				if (disconnectHandler)
					disconnectHandler(ev.peerId);
				break;
		}
	}
}

void ConnectionManager::ioThreadLoop() {
	Threads::ThreadRegistry::instance().registerCurrent("Net-IO");
	BT_LOG("ConnectionManager: I/O thread started");

	while (ioRunning.load(std::memory_order::relaxed)) {
		pollUDP();

		if (tcpListenSocket)
			pollTCPAccept();

		pollTCP();
		sendHeartbeats();

		{
			std::lock_guard<std::mutex> lock(peerMutex);
			for (auto& peer : peers) {
				if (peer.udpConnected)
					peer.udpChannel.retransmitPending(*udpSocket, peer.udpAddress);
			}
		}

		SDL_DelayNS(static_cast<Uint64>(cfg.pollIntervalMicros) * 1000ULL);
	}

	Threads::ThreadRegistry::instance().unregisterCurrent();
	BT_LOG("ConnectionManager: I/O thread stopped");
}

void ConnectionManager::pollUDP() {
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
				"ConnectionManager: Dropped undersized UDP datagram "
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
		bool rateLimitDrop = false;

		{
			std::lock_guard<std::mutex> lock(peerMutex);
			peerId = findOrCreatePeer(srcAddress, false);
			if (peerId == Connection::INVALID_PEER_ID)
				continue;

			auto& peer = peers[peerId];

			if (peer.state == Connection::PeerState::Connecting && !peer.tcpSocket) {
				peer.state = Connection::PeerState::Connected;
				newUDPPeer = true;

				BT_LOG(
					"ConnectionManager: UDP peer {} connected from {}",
					peerId, srcAddress.toString()
				);
			}

			const Connection::RateLimitStage rlStage = peer.rateLimiter.update(bytesRead);

			switch (rlStage) {
				case Connection::RateLimitStage::Disconnect: {
					BT_WARN(
						"ConnectionManager: Peer {} force-disconnected for UDP "
						"rate abuse — peak {:.0f} pkts/s, {:.0f} KB/s, "
						"violation sustained {}ms",
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
					addressToPeerTCP.erase(peer.tcpAddress);
					addressToPeerUDP.erase(peer.udpAddress);

					kickedId = peerId;
					newUDPPeer = false;
					break;
				}

				case Connection::RateLimitStage::Warn: {
					if (peer.rateLimiter.shouldWarn()) {
						BT_WARN(
							"ConnectionManager: Peer {} UDP rate limit — "
							"{:.0f} pkts/s, {:.0f} KB/s (dropping)",
							peer.id,
							peer.rateLimiter.peakPacketRate,
							peer.rateLimiter.peakByteRate / 1024.0f
						);
					}

					rateLimitDrop = true;
					break;
				}

				case Connection::RateLimitStage::Drop: {
					rateLimitDrop = true;
					break;
				}

				default: {
					peer.udpChannel.processInboundHeader(udpHdr);
					peer.markAlive();
					break;
				}
			}
		}

		if (newUDPPeer)
			pushEvent({ ConnectionEventType::Connect, peerId, srcAddress });

		if (kickedId != Connection::INVALID_PEER_ID) {
			pushEvent({ ConnectionEventType::Disconnect, kickedId, {} });
			continue;
		}

		if (rateLimitDrop)
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

		if (!inboundQueue.push(std::move(pkt)))
			BT_WARN("ConnectionManager: Inbound queue full — UDP packet dropped");
	}
}

void ConnectionManager::pollTCPAccept() {
	if (!tcpListenSocket)
		return;

	Transport::Address clientAddr;
	auto clientSocket = tcpListenSocket->accept(clientAddr);
	if (!clientSocket)
		return;

	Connection::PeerId peerId = Connection::INVALID_PEER_ID;

	{
		std::lock_guard<std::mutex> lock(peerMutex);
		peerId = allocatePeerSlot(clientAddr, true);

		if (peerId == Connection::INVALID_PEER_ID) {
			BT_WARN(
				"ConnectionManager: TCP connection from {} rejected — no free slots",
				clientAddr.toString()
			);

			clientSocket->close();

			return;
		}

		auto& peer = peers[peerId];
		peer.tcpSocket = std::move(clientSocket);
		peer.tcpChannel = std::make_unique<Transport::Channels::TCPChannel>();

		peer.markAlive();
	}

	BT_LOG("ConnectionManager: TCP accepted from {} (peerId {})",
		clientAddr.toString(), peerId);
}

void ConnectionManager::pollTCP() {
	struct DeferredEvent {
		ConnectionEventType type;
		Connection::PeerId peerId;
		Transport::Address address;
	};

	std::vector<DeferredEvent> deferred;

	{
		std::lock_guard<std::mutex> lock(peerMutex);

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
				reqHdr.serialize(reqBuf);
				peer.tcpChannel->send(*peer.tcpSocket, reqBuf);
				peer.sentConnectRequest = true;
				BT_LOG("ConnectionManager: sent ConnectRequest to peer {}", peer.id);
			}

			Core::ByteBuffer msg;
			while (peer.tcpChannel->receive(*peer.tcpSocket, msg)) {
				peer.markAlive();

				Protocol::PacketHeader header;
				header.deserialize(msg);

				switch (header.packetType) {
					case Protocol::PacketType::ConnectRequest: {
						if (peer.state == Connection::PeerState::Connecting) {
							Core::ByteBuffer ackBuf;
							Protocol::PacketHeader ackHdr;
							ackHdr.packetType = Protocol::PacketType::ConnectAck;
							ackHdr.serialize(ackBuf);
							peer.tcpChannel->send(*peer.tcpSocket, ackBuf);

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
								"ConnectionManager: Peer {} handshake complete",
								peer.id
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
								"ConnectionManager: Peer {} handshake complete",
								peer.id
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
							Transport::Address::fromIPv4(peer.tcpAddress.ip(), remoteUDPPort);

						if (peer.udpConnected)
							addressToPeerUDP.erase(peer.udpAddress);

						peer.udpAddress = remoteUDP;
						peer.udpConnected = true;
						addressToPeerUDP[remoteUDP] = peer.id;

						BT_LOG("ConnectionManager: Peer {} UDP registered as {}",
							peer.id, remoteUDP.toString());
						break;
					}

					case Protocol::PacketType::Heartbeat: {
						Core::ByteBuffer buf;
						Protocol::PacketHeader hdr;
						hdr.packetType = Protocol::PacketType::HeartbeatAck;
						hdr.serialize(buf);
						peer.tcpChannel->send(*peer.tcpSocket, buf);
						BT_LOG("ConnectionManager: Heartbeat from peer {}", peer.id);
						break;
					}

					case Protocol::PacketType::HeartbeatAck: {
						BT_LOG("ConnectionManager: HeartbeatAck from peer {}", peer.id);
						break;
					}

					default: {
						const Connection::RateLimitStage rlStage =
							peer.rateLimiter.update(msg.size());

						if (rlStage == Connection::RateLimitStage::Disconnect) {
							BT_WARN(
								"ConnectionManager: Peer {} force-disconnected "
								"for TCP rate abuse — peak {:.0f} pkts/s, "
								"{:.0f} KB/s, violation sustained {}ms",
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
							addressToPeerTCP.erase(peer.tcpAddress);
							addressToPeerUDP.erase(peer.udpAddress);

							deferred.push_back({
								ConnectionEventType::Disconnect,
								peer.id,
								{}
							});

							break;
						}

						if (rlStage == Connection::RateLimitStage::Warn) {
							if (peer.rateLimiter.shouldWarn()) {
								BT_WARN(
									"ConnectionManager: Peer {} TCP rate limit "
									"— {:.0f} pkts/s, {:.0f} KB/s (dropping)",
									peer.id,
									peer.rateLimiter.peakPacketRate,
									peer.rateLimiter.peakByteRate / 1024.0f
								);
							}

							break;
						}

						if (rlStage == Connection::RateLimitStage::Drop)
							break;

						Transport::InboundPacket pkt;
						pkt.source = peer.tcpAddress;
						pkt.data = Core::ByteBuffer(msg.data(), msg.size());
						pkt.channel = Transport::InboundPacket::Channel::TCP;
						pkt.peerId = peer.id;

						if (!inboundQueue.push(std::move(pkt)))
							BT_WARN("ConnectionManager: Inbound queue full — "
								"TCP packet dropped");
						break;
					}
				}
			}
		}
	}

	for (const auto& d : deferred)
		pushEvent({ d.type, d.peerId, d.address });
}

void ConnectionManager::checkTimeouts() {
	std::vector<Connection::PeerId> timedOut;

	{
		std::lock_guard<std::mutex> lock(peerMutex);

		for (auto& peer : peers) {
			if (!peer.isConnected())
				continue;

			if (!peer.isTimedOut())
				continue;

			BT_WARN("ConnectionManager: Peer {} timed out", peer.id);
			timedOut.push_back(peer.id);

			if (peer.tcpSocket)
				peer.tcpSocket->close();

			peer.state = Connection::PeerState::Disconnected;
			peer.tcpConnected = false;
			peer.udpConnected = false;
			addressToPeerTCP.erase(peer.tcpAddress);
			addressToPeerUDP.erase(peer.udpAddress);
		}
	}

	for (Connection::PeerId id : timedOut)
		pushEvent({ ConnectionEventType::Disconnect, id, {} });
}

Connection::PeerId ConnectionManager::findPeer(const Transport::Address& address, bool tcp) {
	if (tcp) {
		auto it = addressToPeerTCP.find(address);
		return it != addressToPeerTCP.end() ? it->second : Connection::INVALID_PEER_ID;
	} else {
		auto it = addressToPeerUDP.find(address);
		return it != addressToPeerUDP.end() ? it->second : Connection::INVALID_PEER_ID;
	}
}

Connection::PeerId ConnectionManager::findOrCreatePeer(const Transport::Address& address, bool tcp) {
	Connection::PeerId id = findPeer(address, tcp);
	if (id != Connection::INVALID_PEER_ID)
		return id;

	if (!tcp && !cfg.allowUDPImplicitPeers)
		return Connection::INVALID_PEER_ID;

	return allocatePeerSlot(address, tcp);
}

Connection::PeerId ConnectionManager::allocatePeerSlot(const Transport::Address& address, bool tcp) {
	for (Uint32 i = 0; i < static_cast<Uint32>(peers.size()); ++i) {
		if (peers[i].state != Connection::PeerState::Disconnected)
			continue;

		peers[i].id = i;
		peers[i].state = Connection::PeerState::Connecting;
		peers[i].rateLimiter = Connection::PeerRateLimiter(cfg.rateLimitDefaults);

		if (tcp) {
			peers[i].tcpAddress = address;
			addressToPeerTCP[address] = i;
		} else {
			peers[i].udpAddress = address;
			addressToPeerUDP[address] = i;
		}

		return i;
	}

	BT_WARN("ConnectionManager: No free peer slots");
	return Connection::INVALID_PEER_ID;
}

void ConnectionManager::freePeerSlot(Connection::PeerId id) {
	if (id >= peers.size())
		return;

	auto& peer = peers[id];

	if (peer.tcpSocket)
		peer.tcpSocket->close();

	addressToPeerTCP.erase(peer.tcpAddress);
	addressToPeerUDP.erase(peer.udpAddress);

	peer = Connection::NetworkPeer{};
}

const Connection::NetworkPeer* ConnectionManager::getPeer(Connection::PeerId id) const {
	std::lock_guard<std::mutex> lock(peerMutex);

	if (id >= peers.size())
		return nullptr;

	return &peers[id];
}

std::vector<Connection::NetworkPeer> ConnectionManager::getPeerSnapshot() const {
	std::lock_guard<std::mutex> lock(peerMutex);

	std::vector<Connection::NetworkPeer> snapshot;
	snapshot.reserve(peers.size());

	for (const auto& p : peers) {
		Connection::NetworkPeer copy;
		copy.id = p.id;
		copy.tcpAddress = p.tcpAddress;
		copy.udpAddress = p.udpAddress;
		copy.state = p.state;
		copy.udpConnected = p.udpConnected;
		copy.tcpConnected = p.tcpConnected;
		copy.lastReceivedMs = p.lastReceivedMs;
		copy.timeoutMs = p.timeoutMs;
		copy.label = p.label;

		snapshot.push_back(std::move(copy));
	}

	return snapshot;
}

size_t ConnectionManager::connectedPeerCount() const {
	std::lock_guard<std::mutex> lock(peerMutex);

	size_t count = 0;
	for (const auto& p : peers)
		if (p.isConnected())
			++count;

	return count;
}

void ConnectionManager::sendHeartbeats() {
	if (cfg.heartbeatIntervalMs == 0)
		return;

	std::lock_guard<std::mutex> lock(peerMutex);

	for (auto& peer : peers) {
		if (!peer.needsHeartbeat(cfg.heartbeatIntervalMs))
			continue;

		Core::ByteBuffer buf;
		Protocol::PacketHeader hdr;
		hdr.packetType = Protocol::PacketType::Heartbeat;
		hdr.serialize(buf);

		peer.tcpChannel->send(*peer.tcpSocket, buf);
		peer.lastHeartbeatSentMs = SDL_GetTicks();

		BT_DEBUG("ConnectionManager: Sent Heartbeat to peer {}", peer.id);
	}
}

void ConnectionManager::setPeerRateLimit(Connection::PeerId peerId, const Connection::RateLimitConfig& config) {
	std::lock_guard<std::mutex> lock(peerMutex);

	if (peerId >= peers.size())
		return;

	peers[peerId].rateLimiter.cfg = config;
}

} // namespace Blackthorn::Net