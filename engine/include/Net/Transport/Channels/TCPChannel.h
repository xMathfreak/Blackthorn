#pragma once

#include <vector>

#include <SDL3/SDL.h>

#include "Core/Export.h"
#include "Net/Core/ByteBuffer.h"
#include "Net/Transport/Sockets/ISocket.h"

namespace Blackthorn::Net::Transport::Channels {

/**
 * @brief Per-peer TCP session channel with 4-byte length-prefix framing.
 *
 * @details Raw TCP is a byte stream — it provides no message boundaries.
 * @c TCPChannel adds a simple framing layer where every message is preceded
 * by a 4-byte little-endian unsigned integer representing the message size.
 *
 * @section Wire layout per message
 *
 * @code
 * [uint32 length]  — byte count of the following payload
 * [length bytes]   — PacketHeader + message payload
 * @endcode
 *
 * @section Reading
 *
 * @c TCPChannel maintains an internal receive buffer to handle partial reads,
 * which are common with non-blocking sockets. The I/O thread calls
 * @c receive() each poll iteration. When a complete message has been
 * assembled, @c receive() returns true and populates an output
 * @c ByteBuffer.
 *
 * Multiple complete messages may be assembled in a single call; the caller
 * should loop until @c receive() returns false.
 *
 * @section Writing
 *
 * @c send() writes the length prefix followed by the payload, preferably in a
 * single @c send() syscall where possible.
 */
class BLACKTHORN_API TCPChannel {
public:
	/// Maximum message length accepted. Prevents runaway memory allocation
	/// if a malformed or malicious length prefix is received.
	static constexpr Uint32 MAX_MESSAGE_SIZE = 1u << 20;

	TCPChannel() = default;

	TCPChannel(const TCPChannel&) = delete;
	TCPChannel& operator=(const TCPChannel&) = delete;

	TCPChannel(TCPChannel&&) = default;
	TCPChannel& operator=(TCPChannel&&) = default;

	/**
	 * @brief Sends `payload` over `socket` with a 4-byte length prefix.
	 *
	 * @param socket  Open, connected TCP socket.
	 * @param payload Bytes to send. The length prefix is prepended
	 *                automatically — do not include it in `payload`.
	 * @return SocketResult of the underlying send call.
	 */
	Sockets::SocketResult send(Sockets::ISocket& socket, const Core::ByteBuffer& payload);

	/**
	 * @brief Reads available bytes from `socket` and assembles complete
	 * messages.
	 *
	 * Call this in a loop until it returns false:
	 * @code
	 * Core::ByteBuffer msg;
	 * while (channel.receive(socket, msg)) {
	 *     // msg contains one complete message (without the length prefix)
	 *     queue.push(peerId, std::move(msg));
	 * }
	 * @endcode
	 *
	 * @param socket    Connected TCP socket to read from.
	 * @param outMessage Receives the next complete message on return of true.
	 * @return true if a complete message was assembled, false if more data
	 *         is needed or if the socket would block.
	 */
	bool receive(Sockets::ISocket& socket, Core::ByteBuffer& outMessage);

	/** @brief Returns true if the channel has partial data buffered. */
	bool hasPendingData() const noexcept {
		return !recvBuffer.empty();
	}

private:
	/// Partial receive buffer. Bytes accumulate here until a full message
	/// can be extracted.
	std::vector<Uint8> recvBuffer;

	/// Expected payload size of the message currently being assembled,
	/// or 0 if the length prefix has not yet been fully received.
	Uint32 pendingMessageSize = 0;

	/// Temporary send buffer reused across calls to avoid repeated allocation.
	std::vector<Uint8> sendBuffer;

	static constexpr size_t LENGTH_PREFIX_SIZE = sizeof(Uint32);
};

} // namespace Blackthorn::Net::Transport::Channels