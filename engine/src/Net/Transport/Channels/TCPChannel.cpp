#include "Net/Transport/Channels/TCPChannel.h"

#include "Debug/Logger.h"

namespace Blackthorn::Net::Transport::Channels {

Sockets::SocketResult TCPChannel::send(Sockets::ISocket& socket, const Core::ByteBuffer& payload) {
	const Uint32 len = static_cast<Uint32>(payload.size());

	sendBuffer.clear();
	sendBuffer.reserve(LENGTH_PREFIX_SIZE + payload.size());

	sendBuffer.push_back(static_cast<Uint8>(len));
	sendBuffer.push_back(static_cast<Uint8>(len >> 8));
	sendBuffer.push_back(static_cast<Uint8>(len >> 16));
	sendBuffer.push_back(static_cast<Uint8>(len >> 24));

	sendBuffer.insert(
		sendBuffer.end(),
		payload.data(),
		payload.data() + payload.size()
	);

	size_t totalSent = 0;
	const size_t totalSize = sendBuffer.size();

	while (totalSent < totalSize) {
		size_t sentNow = 0;

		Sockets::SocketResult result = socket.send(
			sendBuffer.data() + totalSent,
			totalSize - totalSent,
			sentNow
		);

		if (result == Sockets::SocketResult::WouldBlock)
			return Sockets::SocketResult::WouldBlock;

		if (result != Sockets::SocketResult::Ok)
			return result;

		totalSent += sentNow;
	}

	return Sockets::SocketResult::Ok;
}

ReceiveResult TCPChannel::receive(Sockets::ISocket& socket,	Core::ByteBuffer& outMessage) {
	const bool havePrefix = pendingMessageSize > 0;
	const bool haveEnough = havePrefix
		? (available() >= pendingMessageSize)
		: (available() >= LENGTH_PREFIX_SIZE);

	if (!haveEnough) {
		Uint8 chunk[4096];
		size_t bytesRead = 0;

		Sockets::SocketResult result =
			socket.recv(chunk, sizeof(chunk), bytesRead);

		if (result == Sockets::SocketResult::Disconnected || result == Sockets::SocketResult::Error)
			return ReceiveResult::FatalError;

		if (bytesRead > 0)
			recvBuffer.insert(recvBuffer.end(), chunk, chunk + bytesRead);
	}

	if (pendingMessageSize == 0) {
		if (available() < LENGTH_PREFIX_SIZE)
			return ReceiveResult::NeedMore;

		const Uint8* p = recvBuffer.data() + readHead;
		const Uint32 len = static_cast<Uint32>(p[0])
						| (static_cast<Uint32>(p[1]) << 8)
						| (static_cast<Uint32>(p[2]) << 16)
						| (static_cast<Uint32>(p[3]) << 24);

		if (len == 0 || len > MAX_MESSAGE_SIZE) {
			BT_WARN(
				"TCPChannel: invalid length prefix {} "
				"(max {}) — stream desynced, disconnecting",
				len, MAX_MESSAGE_SIZE
			);

			return ReceiveResult::FatalError;
		}

		consume(LENGTH_PREFIX_SIZE);
		pendingMessageSize = len;
	}

	if (available() < pendingMessageSize)
		return ReceiveResult::NeedMore;

	outMessage = Core::ByteBuffer(
		recvBuffer.data() + readHead,
		pendingMessageSize
	);

	consume(pendingMessageSize);
	pendingMessageSize = 0;

	maybeCompact();

	return ReceiveResult::Message;
}

} // namespace Blackthorn::Net::Transport::Channels