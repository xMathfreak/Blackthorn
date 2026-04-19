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

bool TCPChannel::receive(Sockets::ISocket& socket, Core::ByteBuffer& outMessage) {
	bool needMoreData =
		(pendingMessageSize == 0 && recvBuffer.size() < LENGTH_PREFIX_SIZE) ||
		(pendingMessageSize >  0 && recvBuffer.size() < pendingMessageSize);

	if (needMoreData) {
		Uint8 chunk[4096];
		size_t bytesRead = 0;

		Sockets::SocketResult result = socket.recv(chunk, sizeof(chunk), bytesRead);

		if (result == Sockets::SocketResult::WouldBlock) {
			// No new data available
		} else if (result == Sockets::SocketResult::Disconnected || result == Sockets::SocketResult::Error) {
			return false;
		} else if (bytesRead > 0) {
			recvBuffer.insert(recvBuffer.end(), chunk, chunk + bytesRead);
		}
	}

	if (pendingMessageSize == 0) {
		if (recvBuffer.size() < LENGTH_PREFIX_SIZE)
			return false;

		Uint32 len =
			static_cast<Uint32>(recvBuffer[0])
			| (static_cast<Uint32>(recvBuffer[1]) << 8)
			| (static_cast<Uint32>(recvBuffer[2]) << 16)
			| (static_cast<Uint32>(recvBuffer[3]) << 24);

		if (len == 0 || len > MAX_MESSAGE_SIZE) {
			BT_WARN("TCPChannel: invalid message length {} — ignoring", len);
			return false;
		}

		pendingMessageSize = len;
		recvBuffer.erase(recvBuffer.begin(), recvBuffer.begin() + LENGTH_PREFIX_SIZE);
	}

	if (recvBuffer.size() < pendingMessageSize)
		return false;

	outMessage = Core::ByteBuffer(recvBuffer.data(), pendingMessageSize);
	recvBuffer.erase(recvBuffer.begin(), recvBuffer.begin() + pendingMessageSize);
	pendingMessageSize = 0;

	return true;
}

} // namespace Blackthorn::Net::Transport::Channels