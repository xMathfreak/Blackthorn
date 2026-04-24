#include "Net/Connection/PeerRegistry.h"

#include "Debug/Logger.h"
#include "Net/Transport/Channels/UDPChannel.h"

namespace Blackthorn::Net::Connection {

void PeerRegistry::init(
	size_t maxPeers,
	const RateLimitConfig& defaults,
	size_t& globalFragmentBytes
) {
	std::lock_guard<std::mutex> lock(peerMutex);
	peers.resize(maxPeers);
	rateLimitDefaults = defaults;
	globalFragmentBytesPtr = &globalFragmentBytes;
}

void PeerRegistry::reset() {
	std::lock_guard<std::mutex> lock(peerMutex);

	for (auto& peer : peers) {
		if (peer.tcpSocket)
			peer.tcpSocket->close();

		peer.state = PeerState::Disconnected;
	}

	addressToPeerTCP.clear();
	addressToPeerUDP.clear();
}

PeerId PeerRegistry::findPeer(const Transport::Address& address, bool tcp) const {
	if (tcp) {
		auto it = addressToPeerTCP.find(address);
		return it != addressToPeerTCP.end() ? it->second : INVALID_PEER_ID;
	} else {
		auto it = addressToPeerUDP.find(address);
		return it != addressToPeerUDP.end() ? it->second : INVALID_PEER_ID;
	}
}

PeerId PeerRegistry::findOrCreate(
	const Transport::Address& address,
	bool tcp,
	bool allowImplicitPeers
) {
	PeerId id = findPeer(address, tcp);
	if (id != INVALID_PEER_ID)
		return id;

	if (!tcp && !allowImplicitPeers)
		return INVALID_PEER_ID;

	return allocateSlot(address, tcp);
}

PeerId PeerRegistry::allocateSlot(const Transport::Address& address, bool tcp) {
	for (Uint32 i = 0; i < static_cast<Uint32>(peers.size()); ++i) {
		if (peers[i].state != PeerState::Disconnected)
			continue;

		peers[i].id = i;
		peers[i].state = PeerState::Connecting;
		peers[i].rateLimiter = PeerRateLimiter(rateLimitDefaults);

		if (globalFragmentBytesPtr) {
			peers[i].fragmentAssembler =
				std::make_unique<Protocol::FragmentAssembler>(
					*globalFragmentBytesPtr
				);
		}

		if (tcp) {
			peers[i].tcpAddress = address;
			addressToPeerTCP[address] = i;
		} else {
			peers[i].udpAddress = address;
			addressToPeerUDP[address] = i;
		}

		return i;
	}

	BT_WARN("PeerRegistry: No free peer slots");
	return INVALID_PEER_ID;
}

void PeerRegistry::freeSlot(PeerId id) {
	if (id >= peers.size())
		return;

	auto& peer = peers[id];

	if (peer.tcpSocket)
		peer.tcpSocket->close();

	if (peer.tcpChannel)
		peer.tcpChannel->reset();

	if (peer.fragmentAssembler)
		peer.fragmentAssembler->reset();

	addressToPeerTCP.erase(peer.tcpAddress);
	addressToPeerUDP.erase(peer.udpAddress);

	peer = NetworkPeer{};
}

bool PeerRegistry::sendUDP(
	PeerId peerId,
	const Core::ByteBuffer& payload,
	Transport::Sockets::UDPSocket& socket
) {
	std::lock_guard<std::mutex> lock(peerMutex);

	if (peerId >= peers.size())
		return false;

	auto& peer = peers[peerId];
	if (!peer.udpConnected)
		return false;

	auto result = peer.udpChannel.send(socket, peer.udpAddress, payload);

	if (result != Transport::Sockets::SocketResult::Ok)
		BT_WARN("PeerRegistry: UDP send failed to {}", peer.udpAddress.toString());

	return result == Transport::Sockets::SocketResult::Ok;
}

bool PeerRegistry::sendTCP(PeerId peerId, const Core::ByteBuffer& payload) {
	std::lock_guard<std::mutex> lock(peerMutex);

	if (peerId >= peers.size())
		return false;

	auto& peer = peers[peerId];
	if (!peer.isConnected() || !peer.tcpSocket || !peer.tcpChannel)
		return false;

	auto result = peer.tcpChannel->send(*peer.tcpSocket, payload);
	return result == Transport::Sockets::SocketResult::Ok;
}

void PeerRegistry::broadcastUDP(
	const Core::ByteBuffer& payload,
	Transport::Sockets::UDPSocket& socket
) {
	std::lock_guard<std::mutex> lock(peerMutex);

	for (auto& peer : peers) {
		if (peer.udpConnected)
			peer.udpChannel.send(socket, peer.udpAddress, payload);
	}
}

void PeerRegistry::broadcastTCP(const Core::ByteBuffer& payload) {
	std::lock_guard<std::mutex> lock(peerMutex);

	for (auto& peer : peers) {
		if (peer.isConnected() && peer.tcpSocket && peer.tcpChannel)
			peer.tcpChannel->send(*peer.tcpSocket, payload);
	}
}

void PeerRegistry::setRateLimit(PeerId peerId, const RateLimitConfig& config) {
	std::lock_guard<std::mutex> lock(peerMutex);

	if (peerId < peers.size())
		peers[peerId].rateLimiter.cfg = config;
}

const NetworkPeer* PeerRegistry::get(PeerId id) const {
	std::lock_guard<std::mutex> lock(peerMutex);

	if (id >= peers.size())
		return nullptr;

	return &peers[id];
}

std::vector<NetworkPeer> PeerRegistry::snapshot() const {
	std::lock_guard<std::mutex> lock(peerMutex);

	std::vector<NetworkPeer> snap;
	snap.reserve(peers.size());

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

		snap.push_back(std::move(copy));
	}

	return snap;
}

size_t PeerRegistry::connectedCount() const {
	std::lock_guard<std::mutex> lock(peerMutex);

	size_t n = 0;
	for (const auto& p : peers)
		if (p.isConnected())
			++n;

	return n;
}

} // namespace Blackthorn::Net::Connection