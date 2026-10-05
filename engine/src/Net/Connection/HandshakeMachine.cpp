#include "Net/Connection/HandshakeMachine.h"

#include <SDL3/SDL.h>

#include "Debug/Logger.h"

namespace Blackthorn::Net::Connection {

void HandshakeMachine::begin(NetworkPeer &peer, PeerOrigin origin) {
	peer.origin = origin;
	peer.handshakePhase = (origin == PeerOrigin::Outbound)
		? HandshakePhase::Dialing
		: HandshakePhase::AwaitingRequest;
	peer.handshakeStartedAtMs = SDL_GetTicks();
}

void HandshakeMachine::onSocketReady(
	NetworkPeer& peer,
	std::vector<HandshakeAction>& out
) {
	// Only the outbound side can send a ConnectRequest.
	if (peer.handshakePhase != HandshakePhase::Dialing)
		return;

	peer.handshakePhase = HandshakePhase::AwaitingAck;

	HandshakeAction act;
	act.type = HandshakeAction::Type::Send;
	act.bytes = makeControlPacket(Protocol::PacketType::ConnectRequest, localVersion);
	out.push_back(std::move(act));

	BT_DEBUG(
		"HandshakeMachine: Peer {} sent ConnectRequest (schema v{})",
		peer.id, localVersion
	);
}

bool HandshakeMachine::isTimedOut(
	const NetworkPeer& peer,
	U64 nowMs,
	U64 timeoutMs
) const noexcept {
	if (timeoutMs == 0)
		return false;

	switch (peer.handshakePhase) {
		case HandshakePhase::Dialing:
		case HandshakePhase::AwaitingRequest:
		case HandshakePhase::AwaitingAck:
			return (nowMs - peer.handshakeStartedAtMs) > timeoutMs;

		default:
			return false;
	}
}

IO::ByteBuffer HandshakeMachine::makeControlPacket(
	Protocol::PacketType type,
	U16 value
) const {
	IO::ByteBuffer buf;

	Protocol::PacketHeader hdr;
	hdr.packetType = type;
	hdr.payloadLength = sizeof(U16);
	hdr.serialize(buf);

	buf.writeU16(value);
	return buf;
}

void HandshakeMachine::reject(
	NetworkPeer& peer,
	std::vector<HandshakeAction>& out,
	const char* reason,
	U16 remoteVersion
) {
	peer.handshakePhase = HandshakePhase::Failed;

	HandshakeAction act;
	act.type = HandshakeAction::Type::Reject;
	act.reason = reason;
	act.schemaVersion = remoteVersion;
	out.push_back(act);
}

bool HandshakeMachine::onPacket(
	NetworkPeer& peer,
	Protocol::PacketType type,
	IO::ByteBuffer& payload,
	U16 localUDPPort,
	std::vector<HandshakeAction>& out
) {
	const bool isHandshakePacket =
		type == Protocol::PacketType::ConnectAck ||
		type == Protocol::PacketType::ConnectRequest;

	switch (peer.handshakePhase) {
		// Server side: waiting for client's request
		case HandshakePhase::AwaitingRequest: {
			if (!isHandshakePacket)
				return false;

			if (type != Protocol::PacketType::ConnectRequest) {
				BT_WARN(
					"HandshakeMachine: Peer {} sent ConnectAck before any ConnectRequest, ignored",
					peer.id
				);
				return true;
			}

			if (payload.remaining() < sizeof(U16)) {
				reject(peer, out, "malformed ConnectRequest", 0);
				return true;
			}

			const U16 clientVersion = payload.readU16();

			if (clientVersion != localVersion) {
				reject(peer, out, "schema version mismatch", clientVersion);
				return true;
			}

			peer.handshakePhase = HandshakePhase::Established;

			HandshakeAction ack;
			ack.type = HandshakeAction::Type::Send;
			ack.bytes = makeControlPacket(Protocol::PacketType::ConnectAck, localVersion);
			out.push_back(std::move(ack));

			HandshakeAction port;
			port.type = HandshakeAction::Type::Send;
			port.bytes = makeControlPacket(Protocol::PacketType::UDPPortInfo, localUDPPort);
			out.push_back(std::move(port));

			HandshakeAction est;
			est.type = HandshakeAction::Type::Established;
			est.schemaVersion = clientVersion;
			out.push_back(std::move(est));

			return true;
		}

		// Client side: request sent, waiting for server ack
		case HandshakePhase::AwaitingAck: {
			if (!isHandshakePacket)
				return false;

			if (type != Protocol::PacketType::ConnectAck) {
				BT_WARN(
					"HandshakeMachine: Peer {} sent ConnectRequest to the dialing side, ignored",
					peer.id
				);
				return true;
			}

			if (payload.remaining() < sizeof(U16)) {
				reject(peer, out, "malformed ConnectAck", 0);
				return true;
			}

			const U16 acceptedVersion = payload.readU16();

			if (acceptedVersion != localVersion) {
				reject(peer, out, "schema version mismatch", acceptedVersion);
				return true;
			}

			peer.handshakePhase = HandshakePhase::Established;

			HandshakeAction port;
			port.type = HandshakeAction::Type::Send;
			port.bytes = makeControlPacket(Protocol::PacketType::UDPPortInfo, localUDPPort);
			out.push_back(std::move(port));

			HandshakeAction est;
			est.type = HandshakeAction::Type::Established;
			est.schemaVersion = acceptedVersion;
			out.push_back(std::move(est));

			return true;
		}

		// Socket still connecting, no packets expected yet.
		case HandshakePhase::Dialing:
			return isHandshakePacket;

		// Late duplicates after completion or failure
		case HandshakePhase::Established:
		case HandshakePhase::Failed: {
			if (isHandshakePacket) {
				BT_TRACE(
					"HandshakeMachine: Ignoring late handshake packet from peer {}",
					peer.id
				);
			}

			return isHandshakePacket;
		}

		// UDP-only peers never run the TCP handshake.
		case HandshakePhase::None:
		default:
			return false;
	}
}

} // namespace Blackthorn::Net::Connection