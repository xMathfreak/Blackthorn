#pragma once

#include <vector>

#include "Core/Export.h"
#include "Types/Numeric.h"
#include "IO/ByteBuffer.h"
#include "Net/Connection/NetworkPeer.h"
#include "Net/Protocol/PacketHeader.h"

namespace Blackthorn::Net::Connection {

/**
 * @brief An effect the handshake state machine asks its caller to perform.
 *
 * @details The machine doesn't directly interact with sockets, channels or
 * the peer registry. It only mutates handshake bookkeeping on the peer and
 * emits a list of actions that the caller executes.
 */
struct HandshakeAction {
	enum class Type : U8 {
		Send, ///< Send `bytes` over the peer's TCP channel.
		Established, ///< Promote the peer to Connected and emit a Connect event.
		Reject ///< Close the socket, mark Disconnected and emit Disconnect.
	};

	/// Fully serialized packet.
	IO::ByteBuffer bytes;

	Type type = Type::Send;

	/// Negotiated schema version.
	U16 schemaVersion = 0;

	/// Rejection reason for logging
	const char* reason = nullptr;
};

/**
 * @brief Connection handshake state machine.
 *
 * The machine is driven by three entry points:
 *
 * - @c begin() — called once when a peer slot is allocated, tagging the
 *   peer with its @c PeerOrigin and entering the initial phase
 *   (Dialing for outbound, AwaitingRequest for inbound).
 * - @c onSocketReady() — called by the I/O thread each poll while an
 *   outbound peer's socket is connecting; emits the ConnectRequest
 *   exactly once.
 * - @c onPacket() — fed every received TCP packet; consumes
 *   ConnectRequest / ConnectAck and validates them against the peer's
 *   current phase and origin.
 *
 * Legal flows:
 *
 * @code
 * Outbound: Dialing --(socket connected)--> AwaitingAck --(valid ConnectAck)--> Established
 * Inbound:  AwaitingRequest --(valid ConnectRequest)--> Established
 * UDP-only peers stay in HandshakePhase::None and never run this machine.
 * @endcode
 *
 * Any other combination (e.g. an inbound peer receiving ConnectAck, or
 * the dialing side receiving ConnectRequest) is a protocol violation and
 * is consumed and ignored, never dispatched to the application.
 *
 * @par Threading
 * Not internally synchronized. All methods are called from the I/O
 * thread while @c PeerRegistry::mutex() is held. @c begin() may also be
 * called from the simulation thread inside
 * @c ConnectionManager::connect(), which holds the same mutex.
 */
class BLACKTHORN_API HandshakeMachine {
public:
	explicit HandshakeMachine(
		U16 localSchemaVersion = Protocol::CURRENT_SCHEMA_VERSION
	) : localVersion(localSchemaVersion) {}


	/**
	 * @brief Initializes handshake bookkeeping on a freshly allocated peer.
	 *
	 * Sets @c origin, enters the initial phase for that origin, and arms
	 * the handshake timeout clock.
	 *
	 * Call once per connection attempt:
	 * - @c ConnectionManager::connect() with @c PeerOrigin::Outbound.
	 * - @c NetworkIOWorker::pollTCPAccept() with @c PeerOrigin::Inbound.
	 *
	 * Do not call for UDP-only peers; they remain in
	 * @c HandshakePhase::None.
	 */
	static void begin(NetworkPeer& peer, PeerOrigin origin);

	/**
	 * @brief Advances an outbound peer whose TCP socket is now connected.
	 *
	 * Emits a single @c Send(ConnectRequest) action and transitions
	 * Dialing -> AwaitingAck. No-op in any other phase, so callers may
	 * invoke it unconditionally on every poll.
	 */
	void onSocketReady(NetworkPeer& peer, std::vector<HandshakeAction>& out);

	/**
	 * @brief Feeds a received TCP packet into the machine.
	 *
	 * @param peer         The peer the packet arrived from.
	 * @param type         Deserialized packet type.
	 * @param payload      Packet buffer positioned after the header.
	 * @param localUDPPort Our own UDP port, advertised via UDPPortInfo
	 *                     when the handshake completes.
	 * @param out          Actions for the caller to execute, in order.
	 *
	 * @return true if the packet was consumed by the handshake layer
	 * (ConnectRequest / ConnectAck in any phase); false if the caller
	 * should continue with normal session-level dispatch.
	 */
	bool onPacket(
		NetworkPeer& peer,
		Protocol::PacketType type,
		IO::ByteBuffer& payload,
		U16 localUDPPort,
		std::vector<HandshakeAction>& out
	);

	/**
	 * @brief Returns true if the peer's handshake has been in progress for
	 * longer than @p timeoutMs without completing.
	 *
	 * Peers in phase None (UDP-only) or Established never time out.
	 * A @p timeoutMs of 0 disables the check.
	 */
	bool isTimedOut(const NetworkPeer& peer, U64 nowMs, U64 timeoutMs) const noexcept;

private:
	/// Builds a fully serialized control packet with a single U16 payload.
	IO::ByteBuffer makeControlPacket(Protocol::PacketType type, U16 value) const;

	/// Transitions the peer to Failed and emits a Reject action.
	static void reject(
		NetworkPeer& peer,
		std::vector<HandshakeAction>& out,
		const char* reason,
		U16 remoteVersion
	);

	U16 localVersion;
};

} // namespace Blackthorn::Net::Connection