#pragma once

#include <mutex>
#include <unordered_map>
#include <vector>

#include "Core/Export.h"
#include "Net/Connection/NetworkPeer.h"
#include "Net/Transport/Address.h"
#include "Net/Transport/Sockets/UDPSocket.h"

namespace Blackthorn::Net {

struct ConnectionConfig;

namespace Connection {

/**
 * @brief Owns and manages the flat array of @c NetworkPeer slots and the
 * two address-to-peer lookup maps.
 *
 * @details All public methods are thread-safe; they acquire @c peerMutex
 * internally. The I/O thread and the simulation thread may call different
 * methods concurrently.
 *
 * @par Peer lifecycle
 *
 * Peers are allocated into a fixed-size flat array by @c allocateSlot().
 * Slots are reused after a peer disconnects - call @c freeSlot() to reset
 * a slot and remove its address mappings.
 *
 * @par Sending
 *
 * Send methods live here rather than in a separate sender class because
 * every send touches peer state (@c udpChannel, @c tcpChannel) under
 * @c peerMutex. Separating them would require exposing the lock or passing
 * many references - both worse than co-location.
 */
class BLACKTHORN_API PeerRegistry {
public:
	explicit PeerRegistry() = default;

	PeerRegistry(const PeerRegistry&) = delete;
	PeerRegistry& operator=(const PeerRegistry&) = delete;

	/**
	 * @brief Allocates peer slots and sets default rate-limit config.
	 *
	 * Must be called before any other method.
	 *
	 * @param maxPeers Maximum simultaneous peers.
	 * @param rateLimitDefaults Default rate-limit config applied to new peers.
	 */
	void init(size_t maxPeers, const RateLimitConfig& rateLimitDefaults);

	/**
	 * @brief Closes all sockets and resets all peer slots.
	 *
	 * Safe to call if @c init() was never called.
	 */
	void reset();

	/**
	 * @brief Looks up a peer by address in the TCP or UDP map.
	 *
	 * @param address Source address.
	 * @param tcp True to query the TCP map, false for the UDP map.
	 * @return PeerId, or @c INVALID_PEER_ID if not found.
	 */
	PeerId findPeer(const Transport::Address& address, bool tcp) const;

	/**
	 * @brief Looks up a peer, allocating a new slot if not found.
	 *
	 * For UDP, respects @c allowUDPImplicitPeers - returns
	 * @c INVALID_PEER_ID without allocating if the flag is false.
	 *
	 * @param address Source address.
	 * @param tcp True for TCP map, false for UDP map.
	 * @param allowImplicitPeers If false, unknown UDP senders are rejected.
	 */
	PeerId findOrCreate(
		const Transport::Address& address,
		bool tcp,
		bool allowImplicitPeers = true
	);

	/**
	 * @brief Allocates a fresh peer slot for @p address.
	 *
	 * Initializes the slot to @c PeerState::Connecting and records the
	 * address in the appropriate lookup map.
	 *
	 * @param address Source/remote address.
	 * @param tcp True if this is a TCP peer; false for UDP-only.
	 * @return Assigned PeerId, or @c INVALID_PEER_ID if the table is full.
	 */
	PeerId allocateSlot(const Transport::Address& address, bool tcp);

	/**
	 * @brief Resets a peer slot and removes its address mappings.
	 *
	 * Closes @c tcpSocket explicitly before the struct reset so the OS
	 * performs a graceful TCP shutdown.
	 *
	 * Caller must hold @c peerMutex.
	 */
	void freeSlot(PeerId id);

	/**
	 * @brief Sends @p payload to @p peerId over UDP.
	 * @return true on success.
	 */
	bool sendUDP(
		PeerId peerId,
		const Core::ByteBuffer& payload,
		Transport::Sockets::UDPSocket& socket
	);

	/**
	 * @brief Sends @p payload to @p peerId over TCP.
	 * @return true on success.
	 */
	bool sendTCP(PeerId peerId, const Core::ByteBuffer& payload);

	/**
	 * @brief Broadcasts @p payload over UDP to all UDP-connected peers.
	 */
	void broadcastUDP(
		const Core::ByteBuffer& payload,
		Transport::Sockets::UDPSocket& socket
	);

	/**
	 * @brief Broadcasts @p payload over TCP to all TCP-connected peers.
	 */
	void broadcastTCP(const Core::ByteBuffer& payload);

	/** Overrides the rate-limit config for a specific peer */
	void setRateLimit(PeerId peerId, const RateLimitConfig& config);

	/** @brief Returns a const pointer to a peer or nullptr. */
	const NetworkPeer* get(PeerId id) const;

	/**
	 * @brief Returns a snapshot of all peer slots.
	 *
	 * The copy omits move-only fields (@c tcpSocket, @c udpSocket,
	 * @c udpChannel). Use @c get() for live socket access.
	 */
	std::vector<NetworkPeer> snapshot() const;

	/** @brief Number of peers currently in the Connected state. */
	size_t connectedCount() const;

	/** @brief Maximum number of peers this registry was initialized for. */
	size_t capacity() const noexcept { return peers.size(); }

	std::vector<NetworkPeer>& peerList() noexcept { return peers; }
	const std::vector<NetworkPeer>& peerList() const noexcept { return peers; }

	std::unordered_map<Transport::Address, PeerId>& tcpMap() noexcept {
		return addressToPeerTCP;
	}

	std::unordered_map<Transport::Address, PeerId>& udpMap() noexcept {
		return addressToPeerUDP;
	}

	std::mutex& mutex() const noexcept { return peerMutex; }

private:
	std::vector<NetworkPeer> peers;
	std::unordered_map<Transport::Address, PeerId> addressToPeerTCP;
	std::unordered_map<Transport::Address, PeerId> addressToPeerUDP;
	mutable std::mutex peerMutex;
	RateLimitConfig rateLimitDefaults;
};

} // namespace Connection

} // namespace Blackthorn::Net