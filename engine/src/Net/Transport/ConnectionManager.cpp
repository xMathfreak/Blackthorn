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
		BT_WARN("ConnectionManager: already running");
		return false;
	}

	cfg = config;
	peers.resize(cfg.maxPeers);
	recvScratch.resize(RECV_BUFFER_SIZE);

	udpSocket = SocketFactory::createUDP();
	if (!udpSocket) {
		BT_ERROR("ConnectionManager: failed to create UDP socket");
		return false;
	}

	udpSocket->setReuseAddr();

	Address udpBind = Address::anyIPv4(cfg.udpPort);
	if (!udpSocket->bind(udpBind)) {
		BT_ERROR("ConnectionManager: failed to bind UDP socket on port {}", cfg.udpPort);
		return false;
	}

	if (cfg.tcpPort > 0) {
		tcpListenSocket = SocketFactory::createTCP();
		if (!tcpListenSocket) {
			BT_ERROR("ConnectionManager: failed to create TCP listen socket");
			return false;
		}

		tcpListenSocket->setReuseAddr();

		Address tcpBind = Address::anyIPv4(cfg.tcpPort);
		if (!tcpListenSocket->bind(tcpBind) || !tcpListenSocket->listen()) {
			BT_ERROR("ConnectionManager: failed to bind/listen TCP on port {}", cfg.tcpPort);
			return false;
		}

		BT_LOG("ConnectionManager: TCP listening on port {}", cfg.tcpPort);
	}

	BT_LOG("ConnectionManager: UDP bound on port {}", cfg.udpPort);

	ioRunning.store(true, std::memory_order::release);
	ioThread = std::thread([this] { ioThreadLoop(); });

	return true;
}

void ConnectionManager::stop() {
	if (!ioRunning.exchange(false, std::memory_order::acq_rel))
		return;

	if (ioThread.joinable())
		ioThread.join();

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

	addressToPeer.clear();
	BT_LOG("ConnectionManager: stopped");
}

PeerId ConnectionManager::connect(const Address& address) {
	std::lock_guard<std::mutex> lock(peerMutex);

	PeerId id = allocatePeerSlot(address);
	if (id == INVALID_PEER_ID) {
		BT_ERROR("ConnectionManager: no free peer slots");
		return INVALID_PEER_ID;
	}

	auto& peer = peers[id];
	peer.udpEnabled = true;

	peer.tcpSocket = SocketFactory::createTCP();
	peer.tcpChannel = std::make_unique<TCPChannel>();

	if (!peer.tcpSocket->connect(address)) {
		BT_ERROR("ConnectionManager: TCP connect to {} failed", address.toString());
		freePeerSlot(id);
		return INVALID_PEER_ID;
	}

	peer.state = PeerState::Connecting;
	peer.touchReceived();

	BT_LOG("ConnectionManager: connecting to {} (peerId {})", address.toString(), id);
	return id;
}

void ConnectionManager::disconnect(PeerId peerId) {
	std::lock_guard<std::mutex> lock(peerMutex);

	if (peerId >= peers.size())
		return;

	auto& peer = peers[peerId];
	if (peer.state == PeerState::Disconnected)
		return;

	if (peer.tcpSocket && peer.tcpChannel && peer.isConnected()) {
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

	addressToPeer.erase(peer.address);
	BT_LOG("ConnectionManager: peer {} disconnected", peerId);
}

bool ConnectionManager::sendUDP(PeerId peerId, const Net::ByteBuffer& payload) {
	std::lock_guard<std::mutex> lock(sendMutex);

	if (peerId >= peers.size())
		return false;

	auto& peer = peers[peerId];
	if (!peer.isConnected() || !peer.udpEnabled)
		return false;

	auto result = peer.udpChannel.send(*udpSocket, peer.address, payload);
	return result == SocketResult::Ok;
}

bool ConnectionManager::sendTCP(PeerId peerId, const Net::ByteBuffer& payload) {
	std::lock_guard<std::mutex> lock(sendMutex);

	if (peerId >= peers.size())
		return false;

	auto& peer = peers[peerId];
	if (!peer.isConnected() || !peer.tcpSocket || !peer.tcpChannel)
		return false;

	auto result = peer.tcpChannel->send(*peer.tcpSocket, payload);
	return result == SocketResult::Ok;
}

void ConnectionManager::broadcastUDP(const Net::ByteBuffer& payload) {
	std::lock_guard<std::mutex> lock(sendMutex);

	for (auto& peer : peers) {
		if (peer.isConnected() && peer.udpEnabled)
			peer.udpChannel.send(*udpSocket, peer.address, payload);
	}
}

void ConnectionManager::broadcastTCP(const Net::ByteBuffer& payload) {
	std::lock_guard<std::mutex> lock(sendMutex);
	for (auto& peer : peers) {
		if (peer.isConnected() && peer.tcpSocket && peer.tcpChannel)
			peer.tcpChannel->send(*peer.tcpSocket, payload);
	}
}

void ConnectionManager::poll(Jobs::JobSystem* jobs) {
	checkTimeouts();

	InboundPacket packet;
	while (inboundQueue.pop(packet)) {
		Net::PacketHeader header;
		header.deserialize(packet.data);

		if (!header.isValid()) {
			BT_WARN(
				"ConnectionManager: invalid packet from peer {} — dropped",
				packet.peerId
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
			jobs->submit(Jobs::Job([pid, hdr, payload = std::move(payload), handler]() mutable {
				handler(pid, hdr, payload);
			}));
		} else {
			handler(pid, hdr, payload);
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

		{
			std::lock_guard<std::mutex> lock(peerMutex);
			for (auto& peer : peers) {
				if (peer.isConnected() && peer.udpEnabled)
					peer.udpChannel.retransmitPending(*udpSocket, peer.address);
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

	Address srcAddress;
	size_t bytesRead = 0;

	SocketResult result = udpSocket->recvFrom(
		recvScratch.data(),
		recvScratch.size(),
		bytesRead,
		srcAddress
	);

	if (result == SocketResult::WouldBlock || bytesRead == 0)
		return;

	if (result != SocketResult::Ok) {
		BT_WARN("ConnectionManager: UDP recvFrom error: {}", udpSocket->getLastError());
		return;
	}

	Net::ByteBuffer datagram(recvScratch.data(), bytesRead);

	if (datagram.remaining() < UDPHeader::SERIALIZED_SIZE)
		return;

	UDPHeader udpHdr;
	udpHdr.deserialize(datagram);

	PeerId peerId;

	{
		std::lock_guard<std::mutex> lock(peerMutex);
		peerId = findOrCreatePeer(srcAddress);
		if (peerId == INVALID_PEER_ID)
			return;

		peers[peerId].udpChannel.processInboundHeader(udpHdr);
		peers[peerId].touchReceived();
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
		BT_WARN("ConnectionManager: inbound queue full — UDP packet dropped");
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
		peerId = allocatePeerSlot(clientAddr);

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
		peer.udpEnabled = true;

		peer.touchReceived();
	}

	BT_LOG("ConnectionManager: TCP accepted from {} (peerId {})",
		clientAddr.toString(), peerId);
}

void ConnectionManager::pollTCP() {
	struct ConnectEvent { PeerId id; Address address; };
	std::vector<ConnectEvent> connectEvents;

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
				BT_LOG("ConnectionManager: TCP connect complete, sent ConnectRequest "
					"to peer {}", peer.id);
			}

			Net::ByteBuffer msg;
			while (peer.tcpChannel->receive(*peer.tcpSocket, msg)) {
				peer.touchReceived();

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
							BT_LOG("ConnectionManager: peer {} handshake complete", peer.id);
							connectEvents.push_back({peer.id, peer.address});
						}
						break;
					}

					case PacketType::ConnectAck: {
						if (peer.state == PeerState::Connecting) {
							peer.state = PeerState::Connected;
							BT_LOG("ConnectionManager: client handshake complete "
								"with peer {}", peer.id);
							connectEvents.push_back({peer.id, peer.address});
						}

						break;
					}

					default: {
						InboundPacket pkt;
						pkt.source = peer.address;
						pkt.data = ByteBuffer(msg.data(), msg.size());
						pkt.channel = InboundPacket::Channel::TCP;
						pkt.peerId = peer.id;

						if (!inboundQueue.push(std::move(pkt)))
							BT_WARN(
								"ConnectionManager: inbound queue full — "
								"TCP packet dropped"
							);

						break;
					}
				}
			}
		}
	}

	if (connectHandler) {
		for (auto& ev : connectEvents)
			connectHandler(ev.id, ev.address);
	}
}

void ConnectionManager::checkTimeouts() {
	std::vector<PeerId> timedOut;

	{
		std::lock_guard<std::mutex> lock(peerMutex);

		for (auto& peer : peers) {
			if (!peer.isConnected())
				continue;

			if (peer.isTimedOut()) {
				BT_WARN("ConnectionManager: peer {} timed out", peer.id);
				timedOut.push_back(peer.id);

				if (peer.tcpSocket)
					peer.tcpSocket->close();

				peer.state = PeerState::Disconnected;
				addressToPeer.erase(peer.address);
			}
		}
	}

	if (disconnectHandler) {
		for (PeerId id : timedOut)
			disconnectHandler(id);
	}
}

PeerId ConnectionManager::findOrCreatePeer(const Address& address) {
	auto it = addressToPeer.find(address);
	if (it != addressToPeer.end())
		return it->second;

	return allocatePeerSlot(address);
}

PeerId ConnectionManager::allocatePeerSlot(const Address& address) {
	for (Uint32 i = 0; i < static_cast<Uint32>(peers.size()); ++i) {
		if (peers[i].state == PeerState::Disconnected) {
			peers[i].id = i;
			peers[i].address = address;
			peers[i].state = PeerState::Connecting;
			addressToPeer[address] = i;

			return i;
		}
	}

	return INVALID_PEER_ID;
}

void ConnectionManager::freePeerSlot(PeerId id) {
	if (id >= peers.size())
		return;

	auto& peer = peers[id];
	addressToPeer.erase(peer.address);
	peer = NetworkPeer{};
	peer.id = INVALID_PEER_ID;
	peer.state = PeerState::Disconnected;
}

const NetworkPeer* ConnectionManager::getPeer(PeerId id) const {
	if (id >= peers.size())
		return nullptr;

	return &peers[id];
}

size_t ConnectionManager::connectedPeerCount() const {
	std::lock_guard<std::mutex> lock(peerMutex);

	size_t count = 0;
	for (const auto& p : peers)
		if (p.isConnected())
			++count;

	return count;
}

} // namespace Blackthorn::Net::Transport