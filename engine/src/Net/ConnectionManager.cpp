#include "Net/ConnectionManager.h"

#include "Debug/Logger.h"
#include "Net/Protocol/PacketHeader.h"
#include "Net/Transport/Channels/TCPChannel.h"
#include "Net/Transport/Sockets/SocketFactory.h"

namespace Blackthorn::Net {

ConnectionManager::ConnectionManager() = default;

ConnectionManager::~ConnectionManager() {
	stop();
}

bool ConnectionManager::start(const ConnectionConfig& cfg) {
	if (ioWorker.isRunning()) {
		BT_WARN("ConnectionManager: Already running");
		return false;
	}

	registry.init(cfg.maxPeers, cfg.rateLimitDefaults, ioWorker.getGlobalFragmentBytes());

	dispatcher.registry = &registry;
	dispatcher.eventBus = &eventBus;

	if (!ioWorker.start(cfg, registry, eventBus, dispatcher.inboundQueue)) {
		BT_ERROR("ConnectionManager: Failed to start I/O worker");
		registry.reset();
		return false;
	}

	BT_LOG("ConnectionManager: Started");
	return true;
}

void ConnectionManager::stop() {
	ioWorker.stop();
	eventBus.clear();
	dispatcher.reset();
	registry.reset();

	BT_LOG("ConnectionManager: Stopped");
}

Connection::PeerId ConnectionManager::connect(const Transport::Address& address) {
	std::lock_guard<std::mutex> lock(registry.mutex());

	Connection::PeerId id = registry.allocateSlot(address, true);
	if (id == Connection::INVALID_PEER_ID) {
		BT_ERROR("ConnectionManager: No free peer slots for {}", address.toString());
		return Connection::INVALID_PEER_ID;
	}

	auto& peer = registry.peerList()[id];

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
	std::lock_guard<std::mutex> lock(registry.mutex());

	if (peerId >= registry.capacity())
		return;

	auto& peer = registry.peerList()[peerId];
	if (peer.state == Connection::PeerState::Disconnected)
		return;

	if (peer.tcpConnected && peer.tcpSocket && peer.tcpChannel) {
		IO::ByteBuffer buf;
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

	registry.tcpMap().erase(peer.tcpAddress);
	registry.udpMap().erase(peer.udpAddress);

	BT_DEBUG("ConnectionManager: Peer {} disconnected", peerId);
}

bool ConnectionManager::sendUDP(
	Connection::PeerId peerId,
	const IO::ByteBuffer& payload
) {
	auto* sock = ioWorker.udpSocketPtr();
	if (!sock)
		return false;

	return registry.sendUDP(peerId, payload, *sock);
}

bool ConnectionManager::sendTCP(
	Connection::PeerId peerId,
	const IO::ByteBuffer& payload
) {
	return registry.sendTCP(peerId, payload);
}

void ConnectionManager::broadcastUDP(const IO::ByteBuffer& payload) {
	auto* sock = ioWorker.udpSocketPtr();
	if (sock)
		registry.broadcastUDP(payload, *sock);
}

void ConnectionManager::broadcastTCP(const IO::ByteBuffer& payload) {
	registry.broadcastTCP(payload);
}

} // namespace Blackthorn::Net