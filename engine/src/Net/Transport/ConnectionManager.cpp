#include "Net/Transport/ConnectionManager.h"

#ifdef _WIN32
	#include <winsock2.h>
#else
	#include <sys/select.h>
#endif

#include "Debug/Logger.h"
#include "Net/PacketHeader.h"
#include "Net/Transport/TCPSocket.h"
#include "Threads/ThreadRegistry.h"
#include "Net/Transport/SocketFactory.h"

namespace Blackthorn::Net::Transport {

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

	udpSocket = SocketFactory::createUDP();
	if (!udpSocket) {
		BT_ERROR("ConnectionManager: Failed to create UDP socket");
		return false;
	}

	Address udpBind = Address::anyIPv4(cfg.udpPort);
	if (!udpSocket->bind(udpBind)) {
		BT_ERROR("ConnectionManager: Failed to bind UDP socket on port {}", cfg.udpPort);
		return false;
	}

	if (cfg.tcpPort > 0) {
		tcpListenSocket = SocketFactory::createTCP();
		if (!tcpListenSocket) {
			BT_ERROR("ConnectionManager: Failed to create TCP listen socket");
			return false;
		}

		Address tcpBind = Address::anyIPv4(cfg.tcpPort);
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

	std::lock_guard<std::mutex> lock(peerMutex);

	for (auto& peer : peers) {
		if (peer.tcpSocket)
			peer.tcpSocket->close();

		peer.state = PeerState::Disconnected;
	}

	if (udpSocket)
		udpSocket->close();

	if (tcpListenSocket)
		tcpListenSocket->close();

	addressToPeerTCP.clear();
	addressToPeerUDP.clear();

	BT_LOG("ConnectionManager: Stopped");
}

PeerId ConnectionManager::connect(const Address& address) {
	std::lock_guard<std::mutex> lock(peerMutex);

	PeerId id = allocatePeerSlot(address, true);
	if (id == INVALID_PEER_ID) {
		BT_ERROR("ConnectionManager: No free peer slots for {}", address.toString());
		return INVALID_PEER_ID;
	}

	auto& peer = peers[id];

	if (peer.tcpConnected) {
		peer.markAlive();
		return id;
	}

	auto tcpSock = SocketFactory::createTCP();
	if (tcpSock && tcpSock->connect(address)) {
		peer.tcpSocket = std::move(tcpSock);
		peer.tcpChannel = std::make_unique<TCPChannel>();
		peer.state = PeerState::Connecting;

		BT_LOG(
			"ConnectionManager: TCP connecting to {} (peerId {})",
			address.toString(), id
		);
	} else {
		peer.state = PeerState::Connecting;
		BT_LOG(
			"ConnectionManager: TCP unavailable for {}; UDP-only (peerId {})",
			address.toString(), id
		);
	}

	peer.markAlive();
	return id;
}

void ConnectionManager::disconnect(PeerId peerId) {
	std::lock_guard<std::mutex> lock(peerMutex);

	if (peerId >= peers.size())
		return;

	auto& peer = peers[peerId];
	if (peer.state == PeerState::Disconnected)
		return;

	if (peer.tcpConnected && peer.tcpSocket && peer.tcpChannel) {
		Net::ByteBuffer buf;
		Net::PacketHeader hdr;
		hdr.packetType = Net::PacketType::Disconnect;
		hdr.tick = 0;
		hdr.payloadLength = 0;
		hdr.serialize(buf);
		peer.tcpChannel->send(*peer.tcpSocket, buf);
	}

	if (peer.tcpSocket)
		peer.tcpSocket->close();

	peer.state = PeerState::Disconnected;
	peer.tcpConnected = false;
	peer.udpConnected = false;

	addressToPeerTCP.erase(peer.tcpAddress);
	addressToPeerUDP.erase(peer.udpAddress);

	BT_LOG("ConnectionManager: Peer {} disconnected", peerId);
}

bool ConnectionManager::sendUDP(PeerId peerId, const Net::ByteBuffer& payload) {
	std::lock_guard<std::mutex> lock(peerMutex);

	if (peerId >= peers.size())
		return false;

	auto& peer = peers[peerId];

	if (!peer.udpConnected)
		return false;

	auto result = peer.udpChannel.send(*udpSocket, peer.udpAddress, payload);

	if (result != SocketResult::Ok)
		BT_WARN("ConnectionManager: UDP send failed to {}", peer.udpAddress.toString());

	return result == SocketResult::Ok;
}

bool ConnectionManager::sendTCP(PeerId peerId, const Net::ByteBuffer& payload) {
	std::lock_guard<std::mutex> lock(peerMutex);

	if (peerId >= peers.size())
		return false;

	auto& peer = peers[peerId];
	if (!peer.isConnected() || !peer.tcpSocket || !peer.tcpChannel)
		return false;

	auto result = peer.tcpChannel->send(*peer.tcpSocket, payload);
	return result == SocketResult::Ok;
}

void ConnectionManager::broadcastUDP(const Net::ByteBuffer& payload) {
	std::lock_guard<std::mutex> lock(peerMutex);

	for (auto& peer : peers) {
		if (peer.udpConnected)
			peer.udpChannel.send(*udpSocket, peer.udpAddress, payload);
	}
}

void ConnectionManager::broadcastTCP(const Net::ByteBuffer& payload) {
	std::lock_guard<std::mutex> lock(peerMutex);

	for (auto& peer : peers) {
		if (peer.isConnected() && peer.tcpSocket && peer.tcpChannel)
			peer.tcpChannel->send(*peer.tcpSocket, payload);
	}
}

void ConnectionManager::poll(Jobs::JobSystem* jobs) {
	checkTimeouts();

	dispatchPendingEvents();

	InboundPacket packet;
	while (inboundQueue.pop(packet)) {
		Net::PacketHeader header;
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

		if (!packetHandler)
			continue;

		PeerId pid = packet.peerId;
		Net::PacketHeader hdr = header;
		Net::ByteBuffer payload = std::move(packet.data);
		auto handler = packetHandler;

		if (jobs) {
			jobs->submit(Jobs::Job(
				[pid, hdr, payload = std::move(payload), handler]() mutable {
					handler(pid, hdr, payload);
				}
			));
		} else {
			handler(pid, hdr, payload);
		}
	}
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
		Address srcAddress;
		size_t bytesRead = 0;

		SocketResult result = udpSocket->recvFrom(
			recvScratch.data(),
			recvScratch.size(),
			bytesRead,
			srcAddress
		);

		if (result == SocketResult::WouldBlock)
			break;

		if (result != SocketResult::Ok || bytesRead == 0)
			break;

		Net::ByteBuffer datagram(recvScratch.data(), bytesRead);

		if (datagram.remaining() < UDPChannel::MIN_DATAGRAM_SIZE) {
			BT_WARN(
				"ConnectionManager: Dropped undersized UDP datagram "
				"({} bytes, minimum {})",
				bytesRead, UDPChannel::MIN_DATAGRAM_SIZE
			);

			continue;
		}

		UDPHeader udpHdr;
		udpHdr.deserialize(datagram);

		PeerId peerId = INVALID_PEER_ID;
		bool newUDPPeer = false;

		{
			std::lock_guard<std::mutex> lock(peerMutex);
			peerId = findOrCreatePeer(srcAddress, false);
			if (peerId == INVALID_PEER_ID)
				continue;

			auto& peer = peers[peerId];

			if (peer.state == PeerState::Connecting && !peer.tcpSocket) {
				peer.state = PeerState::Connected;
				newUDPPeer = true;

				BT_LOG(
					"ConnectionManager: UDP peer {} connected from {}",
					peerId, srcAddress.toString()
				);
			}

			peer.udpChannel.processInboundHeader(udpHdr);
			peer.markAlive();
		}

		if (newUDPPeer) {
			pushEvent({
				ConnectionEventType::Connect,
				peerId,
				srcAddress
			});
		}

		Net::ByteBuffer payload(
			datagram.data() + datagram.readPosition(),
			datagram.remaining()
		);

		InboundPacket pkt;
		pkt.source = srcAddress;
		pkt.data = std::move(payload);
		pkt.channel = InboundPacket::Channel::UDP;
		pkt.peerId = peerId;

		if (!inboundQueue.push(std::move(pkt)))
			BT_WARN("ConnectionManager: Inbound queue full — UDP packet dropped");
	}
}

void ConnectionManager::pollTCPAccept() {
	if (!tcpListenSocket)
		return;

	Address clientAddr;
	auto clientSocket = tcpListenSocket->accept(clientAddr);
	if (!clientSocket)
		return;

	PeerId peerId = INVALID_PEER_ID;

	{
		std::lock_guard<std::mutex> lock(peerMutex);
		peerId = allocatePeerSlot(clientAddr, true);

		if (peerId == INVALID_PEER_ID) {
			BT_WARN(
				"ConnectionManager: TCP connection from {} rejected — no free slots",
				clientAddr.toString()
			);

			clientSocket->close();

			return;
		}

		auto& peer = peers[peerId];
		peer.tcpSocket = std::move(clientSocket);
		peer.tcpChannel = std::make_unique<TCPChannel>();

		peer.markAlive();
	}

	BT_LOG("ConnectionManager: TCP accepted from {} (peerId {})",
		clientAddr.toString(), peerId);
}

void ConnectionManager::pollTCP() {
	struct DeferredEvent {
		ConnectionEventType type;
		PeerId peerId;
		Address address;
	};

	std::vector<DeferredEvent> deferred;

	{
		std::lock_guard<std::mutex> lock(peerMutex);

		for (auto& peer : peers) {
			if (peer.state == PeerState::Disconnected)
				continue;

			if (!peer.tcpSocket || !peer.tcpChannel)
				continue;

			if (peer.state == PeerState::Connecting
				&& !peer.sentConnectRequest
				&& peer.tcpSocket->isConnected())
			{
				ByteBuffer reqBuf;
				PacketHeader reqHdr;
				reqHdr.packetType = PacketType::ConnectRequest;
				reqHdr.serialize(reqBuf);
				peer.tcpChannel->send(*peer.tcpSocket, reqBuf);
				peer.sentConnectRequest = true;
				BT_LOG("ConnectionManager: sent ConnectRequest to peer {}", peer.id);
			}

			Net::ByteBuffer msg;
			while (peer.tcpChannel->receive(*peer.tcpSocket, msg)) {
				peer.markAlive();

				PacketHeader header;
				header.deserialize(msg);

				switch (header.packetType) {
					case PacketType::ConnectRequest: {
						if (peer.state == PeerState::Connecting) {
							ByteBuffer ackBuf;
							PacketHeader ackHdr;
							ackHdr.packetType = PacketType::ConnectAck;
							ackHdr.serialize(ackBuf);
							peer.tcpChannel->send(*peer.tcpSocket, ackBuf);

							peer.state = PeerState::Connected;
							peer.tcpConnected = true;

							ByteBuffer portBuf;
							PacketHeader portHdr;
							portHdr.packetType = PacketType::UDPPortInfo;
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

					case PacketType::ConnectAck: {
						if (peer.state == PeerState::Connecting) {
							peer.state = PeerState::Connected;
							peer.tcpConnected = true;

							ByteBuffer portBuf;
							PacketHeader portHdr;
							portHdr.packetType = PacketType::UDPPortInfo;
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

					case PacketType::UDPPortInfo: {
						const Uint16 remoteUDPPort = msg.readU16();
						const Address remoteUDP =
							Address::fromIPv4(peer.tcpAddress.ip(), remoteUDPPort);

						if (peer.udpConnected)
							addressToPeerUDP.erase(peer.udpAddress);

						peer.udpAddress = remoteUDP;
						peer.udpConnected = true;
						addressToPeerUDP[remoteUDP] = peer.id;

						BT_LOG("ConnectionManager: Peer {} UDP registered as {}",
							peer.id, remoteUDP.toString());
						break;
					}

					case PacketType::Heartbeat: {
						ByteBuffer buf;
						PacketHeader hdr;
						hdr.packetType = PacketType::HeartbeatAck;
						hdr.serialize(buf);
						peer.tcpChannel->send(*peer.tcpSocket, buf);
						BT_LOG("ConnectionManager: Heartbeat from peer {}", peer.id);
						break;
					}

					case PacketType::HeartbeatAck: {
						BT_LOG("ConnectionManager: HeartbeatAck from peer {}", peer.id);
						break;
					}

					default: {
						InboundPacket pkt;
						pkt.source = peer.tcpAddress;
						pkt.data = ByteBuffer(msg.data(), msg.size());
						pkt.channel = InboundPacket::Channel::TCP;
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
	std::vector<PeerId> timedOut;

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

			peer.state = PeerState::Disconnected;
			peer.tcpConnected = false;
			peer.udpConnected = false;
			addressToPeerTCP.erase(peer.tcpAddress);
			addressToPeerUDP.erase(peer.udpAddress);
		}
	}

	for (PeerId id : timedOut)
		pushEvent({ ConnectionEventType::Disconnect, id, {} });
}

PeerId ConnectionManager::findPeer(const Address& address, bool tcp) {
	if (tcp) {
		auto it = addressToPeerTCP.find(address);
		return it != addressToPeerTCP.end() ? it->second : INVALID_PEER_ID;
	} else {
		auto it = addressToPeerUDP.find(address);
		return it != addressToPeerUDP.end() ? it->second : INVALID_PEER_ID;
	}
}

PeerId ConnectionManager::findOrCreatePeer(const Address& address, bool tcp) {
	PeerId id = findPeer(address, tcp);
	if (id != INVALID_PEER_ID)
		return id;

	if (!tcp && !cfg.allowUDPImplicitPeers)
		return INVALID_PEER_ID;

	return allocatePeerSlot(address, tcp);
}

PeerId ConnectionManager::allocatePeerSlot(const Address& address, bool tcp) {
	for (Uint32 i = 0; i < static_cast<Uint32>(peers.size()); ++i) {
		if (peers[i].state != PeerState::Disconnected)
			continue;

		peers[i].id = i;
		peers[i].state = PeerState::Connecting;

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
	return INVALID_PEER_ID;
}

void ConnectionManager::freePeerSlot(PeerId id) {
	if (id >= peers.size())
		return;

	auto& peer = peers[id];

	if (peer.tcpSocket)
		peer.tcpSocket->close();

	addressToPeerTCP.erase(peer.tcpAddress);
	addressToPeerUDP.erase(peer.udpAddress);

	peer = NetworkPeer{};
}

const NetworkPeer* ConnectionManager::getPeer(PeerId id) const {
	std::lock_guard<std::mutex> lock(peerMutex);

	if (id >= peers.size())
		return nullptr;

	return &peers[id];
}

std::vector<NetworkPeer> ConnectionManager::getPeerSnapshot() const {
	std::lock_guard<std::mutex> lock(peerMutex);

	std::vector<NetworkPeer> snapshot;
	snapshot.reserve(peers.size());

	for (const auto& p : peers) {
		NetworkPeer copy;
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

		ByteBuffer buf;
		PacketHeader hdr;
		hdr.packetType = PacketType::Heartbeat;
		hdr.serialize(buf);

		peer.tcpChannel->send(*peer.tcpSocket, buf);
		peer.lastHeartbeatSentMs = SDL_GetTicks();

		BT_DEBUG("ConnectionManager: Sent Heartbeat to peer {}", peer.id);
	}
}

} // namespace Blackthorn::Net::Transport