#pragma once

#include <memory>
#include <string>

#include <SDL3/SDL.h>

#include "Core/Export.h"
#include "Net/Transport/Address.h"
#include "Net/Transport/ISocket.h"
#include "Net/Transport/TCPChannel.h"
#include "Net/Transport/UDPChannel.h"

namespace Blackthorn::Net::Transport {

/** @brief Numeric identifier for a peer, assigned by ConnectionManager. */
using PeerId = Uint32;

static constexpr PeerId INVALID_PEER_ID = 0xFFFFFFFFu;

/**
 * @brief Connection state of a peer.
 */
enum class PeerState : Uint8 {
	Disconnected, ///< No active connection.
	Connecting, ///< Handshake in progress (TCP connect or UDP hello).
	Connected, ///< Fully established — simulation traffic may flow.
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
	Address tcpAddress;

	/// Remote address for UDP connection.
	Address udpAddress;

	/// Current connection state.
	PeerState state = PeerState::Disconnected;

	/// UDP simulation channel. Always allocated; only used when udpConnected.
	UDPChannel udpChannel;

	/// Whether UDP traffic is active for this peer.
	bool udpConnected = false;

	/// Whether TCP traffic is active for this peer.
	bool tcpConnected = false;

	/// TCP session channel. Nullptr if TCP is not used for this peer.
	std::unique_ptr<TCPChannel> tcpChannel;

	/// TCP socket for this peer. Nullptr if TCP is not established.
	std::unique_ptr<ISocket> tcpSocket;

	/// Timestamp of the last received packet (either channel), in ms.
	/// Used for timeout detection.
	Uint64 lastReceivedMs = 0;

	/// Timestamp of the last outbound Heartbeat sent to this peer.
	Uint64 lastHeartbeatSentMs = 0;

	/// Timeout threshold in milliseconds. A peer is considered timed out
	/// when `SDL_GetTicks() - lastReceivedMs > timeoutMs`.
	Uint64 timeoutMs = 10000;

	/// Set to true once the client has sent its ConnectRequest over the
	/// established TCP socket.  Prevents pollTCP() from re-sending it on
	/// every subsequent iteration before the server responds.
	bool sentConnectRequest = false;

	/// Optional human-readable label (e.g. "Player 1", "Server").
	std::string label;

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

	bool needsHeartbeat(Uint32 intervalMs) const noexcept {
		if (intervalMs == 0 || !tcpConnected || tcpSocket == nullptr)
			return false;

		const Uint64 now = SDL_GetTicks();
		const bool silent = (now - lastReceivedMs) >= intervalMs;
		const bool notSent = (now - lastHeartbeatSentMs) >= intervalMs;

		return silent && notSent;
	}

	/** @brief Updates the last-received timestamp to now. */
	void markAlive() noexcept {
		lastReceivedMs = SDL_GetTicks();
	}
};

} // namespace Blackthorn::Net::Transport