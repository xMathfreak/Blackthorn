#pragma once

#include <memory>
#include <string>

#include <SDL3/SDL.h>

#include "Core/Export.h"
#include "Core/Types/Numeric.h"
#include "Net/Connection/PeerRateLimiter.h"
#include "Net/Connection/PeerRateLimiter.h"
#include "Net/Protocol/FragmentAssembler.h"
#include "Net/Transport/Address.h"
#include "Net/Transport/Channels/TCPChannel.h"
#include "Net/Transport/Channels/UDPChannel.h"
#include "Net/Transport/Sockets/ISocket.h"

namespace Blackthorn::Net::Connection {

/** @brief Numeric identifier for a peer, assigned by ConnectionManager. */
using PeerId = U32;

static constexpr PeerId INVALID_PEER_ID = 0xFFFFFFFFu;

/**
 * @brief Connection state of a peer.
 */
enum class PeerState : U8 {
	Disconnected, ///< No active connection.
	Connecting, ///< Handshake in progress (TCP connect or UDP hello).
	Connected, ///< Fully established - simulation traffic may flow.
	Disconnecting, ///< Graceful shutdown in progress.
};

/**
 * @brief A single remote peer holding both a UDP simulation channel and a
 * TCP session channel.
 *
 * @details @c ConnectionManager owns a flat array of @c NetworkPeer instances
 * up to @c maxPeers. Slots are reused after a peer disconnects.
 *
 * @par Channel optionality
 *
 * Either channel may be absent depending on the peer's capabilities:
 *
 * - UDP-only peers: @c tcpSocket is nullptr.
 * - TCP-only peers: @c udpConnected is false (the UDP channel is still allocated
 *   but never used to keep the struct layout predictable).
 *
 * For the standard dual-channel configuration, both channels are active.
 */
struct BLACKTHORN_API NetworkPeer {
	/// Unique ID assigned by ConnectionManager on slot allocation.
	PeerId id = INVALID_PEER_ID;

	/// Remote address for TCP connection.
	Transport::Address tcpAddress;

	/// Remote address for UDP connection.
	Transport::Address udpAddress;

	/// Current connection state.
	PeerState state = PeerState::Disconnected;

	/// UDP simulation channel. Always allocated; only used when udpConnected.
	Transport::Channels::UDPChannel udpChannel;

	/// Whether UDP traffic is active for this peer.
	bool udpConnected = false;

	/// Whether TCP traffic is active for this peer.
	bool tcpConnected = false;

	/// TCP session channel. Nullptr if TCP is not used for this peer.
	std::unique_ptr<Transport::Channels::TCPChannel> tcpChannel;

	/// TCP socket for this peer. Nullptr if TCP is not established.
	std::unique_ptr<Transport::Sockets::ISocket> tcpSocket;

	/// Timestamp of the last received packet (either channel), in ms.
	/// Used for timeout detection.
	U64 lastReceivedMs = 0;

	/// Timestamp of the last outbound Heartbeat sent to this peer.
	U64 lastHeartbeatSentMs = 0;

	/// Timeout threshold in milliseconds. A peer is considered timed out
	/// when `SDL_GetTicks() - lastReceivedMs > timeoutMs`.
	U64 timeoutMs = 10000;

	/// Set to true once the client has sent its ConnectRequest over the
	/// established TCP socket.  Prevents pollTCP() from re-sending it on
	/// every subsequent iteration before the server responds.
	bool sentConnectRequest = false;

	/// Optional human-readable label (e.g. "Player 1", "Server").
	std::string label;

	/// Per-peer inbound rate limiter. Initialised with defaults from
	/// ConnectionConfig; may be overridden after connection via
	/// ConnectionManager::setPeerRateLimit().
	PeerRateLimiter rateLimiter;

	/// Schema version agreed during the TCP handshake.
	/// 0 = not yet negotiated (peer is still Connecting).
	U16 negotiatedSchemaVersion = 0;

	/// Per-peer UDP fragment reassembler.
	/// Initialised by PeerRegistry::allocateSlot() with a reference to
	/// NetworkIOWorker::globalFragmentBytes so the assembler can enforce
	/// the engine-wide memory cap.
	/// nullptr until the peer slot is allocated.
	std::unique_ptr<Protocol::FragmentAssembler> fragmentAssembler;

	/** @brief Returns true if the peer is in the Connected state. */
	bool isConnected() const noexcept {
		return (tcpConnected && tcpSocket != nullptr) || udpConnected;
	}

	/** @brief Returns true if the peer has timed out. */
	bool isTimedOut() const noexcept {
		if (!(state == PeerState::Connected || state == PeerState::Connecting))
			return false;

		return (SDL_GetTicks() - lastReceivedMs) > timeoutMs;
	}

	/**
	 * @brief Returns true if a Heartbeat should be sent to this peer.
	 *
	 * @param intervalMs Interval from @c ConnectionConfig::heartbeatIntervalMs.
	 *
	 * A heartbeat is due when:
	 *   - The peer is fully Connected over TCP (we only heartbeat TCP peers).
	 *   - No data has been received for at least @p intervalMs milliseconds.
	 *   - We have not already sent a heartbeat within the same interval
	 *     (prevents flooding if the remote side stops responding entirely).
	 */
	bool needsHeartbeat(U32 intervalMs) const noexcept {
		if (intervalMs == 0 || !tcpConnected || tcpSocket == nullptr)
			return false;

		const U64 now = SDL_GetTicks();
		const bool silent = (now - lastReceivedMs) >= intervalMs;
		const bool notSent = (now - lastHeartbeatSentMs) >= intervalMs;

		return silent && notSent;
	}

	/** @brief Updates the last-received timestamp to now. */
	void markAlive() noexcept {
		lastReceivedMs = SDL_GetTicks();
	}
};

} // namespace Blackthorn::Net::Connection