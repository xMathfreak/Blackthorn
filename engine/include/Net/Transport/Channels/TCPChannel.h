#pragma once

#include <vector>

#include <SDL3/SDL.h>

#include "Core/Export.h"
#include "IO/ByteBuffer.h"
#include "Net/Transport/Sockets/ISocket.h"

namespace Blackthorn::Net::Transport::Channels {

/**
 * @brief Result of a @c TCPChannel::receive() call.
 *
 * Allows callers to distinguish between "no data yet"
 * and "fatal framing error".
 */
enum class ReceiveResult : Uint8 {
	/// A complete message was assembled and written to @c outMessage.
	Message,

	/// No complete message is available yet. More data is needed,
	/// or the socket would block. This is the normal idle state.
	NeedMore,

	/**
	 * @brief Unrecoverable framing error. The connection must be closed.
	 *
	 * Returned when:
	 * - The length prefix is zero or exceeds @c MAX_MESSAGE_SIZE.
	 * - The underlying socket reports a hard error or remote disconnect.
	 *
	 * The TCP stream is de-synchronized at this point. The caller must
	 * close the socket and evict the peer. Continuing to read will
	 * produce garbage.
	 */
	FatalError,
};

/**
 * @brief Per-peer TCP session channel with 4-byte length-prefix framing.
 *
 * @details Raw TCP is a byte stream - it provides no message boundaries.
 * @c TCPChannel adds a simple framing layer where every message is preceded
 * by a 4-byte little-endian unsigned integer representing the message size.
 *
 * @section Wire layout per message
 *
 * @code
 * [uint32 length]  - byte count of the following payload
 * [length bytes]   - PacketHeader + message payload
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
	static constexpr Uint32 MAX_MESSAGE_SIZE = 1 << 20u;

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
	 *                automatically - do not include it in `payload`.
	 * @return SocketResult of the underlying send call.
	 */
	Sockets::SocketResult send(Sockets::ISocket& socket, const IO::ByteBuffer& payload);

	/**
	 * @brief Reads available bytes from @p socket and assembles complete
	 * messages.
	 *
	 * Call in a loop until the result is not @c ReceiveResult::Message:
	 *
	 * @code
	 * IO::ByteBuffer msg;
	 * ReceiveResult r;
	 * while ((r = channel.receive(socket, msg)) == ReceiveResult::Message) {
	 *     processMessage(msg);
	 * }
	 * if (r == ReceiveResult::FatalError)
	 *     disconnectPeer();
	 * @endcode
	 *
	 * @param socket     Connected TCP socket to read from.
	 * @param outMessage Receives the next complete message when
	 *                   @c ReceiveResult::Message is returned. Unmodified
	 *                   on any other result.
	 * @return @c Message, @c NeedMore, or @c FatalError.
	 */
	ReceiveResult receive(Sockets::ISocket& socket, IO::ByteBuffer& outMessage);

	/** @brief Returns true if the channel has unconsumed buffered bytes. */
	bool hasPendingData() const noexcept {
		return readHead < recvBuffer.size();
	}

	/**
	 * @brief Discards all buffered data and resets framing state.
	 *
	 * Call before reusing a channel slot after a disconnect, so stale
	 * partial message data from a previous connection cannot bleed into
	 * the next one.
	 */
	void reset() noexcept {
		recvBuffer.clear();
		readHead = 0;
		pendingMessageSize = 0;
	}

private:
	/// Partial receive buffer. Bytes accumulate here until a full message
	/// can be extracted.
	std::vector<Uint8> recvBuffer;

	/// index of the first unconsumed byte in @c recvBuffer.
	size_t readHead = 0;

	/// Expected payload size of the message currently being assembled,
	/// or 0 if the length prefix has not yet been fully received.
	Uint32 pendingMessageSize = 0;

	/// Temporary send buffer reused across calls to avoid repeated allocation.
	std::vector<Uint8> sendBuffer;

	static constexpr size_t LENGTH_PREFIX_SIZE = sizeof(Uint32);

	/// Number of unconsumed bytes currently in @c recvBuffer.
	size_t available() const noexcept {
		return recvBuffer.size() - readHead;
	}

	/// Advance the read head by @p count bytes.
	void consume(size_t count) noexcept {
		readHead += count;
	}

	/**
	 * @brief Shifts unconsumed bytes to the front of @c recvBuffer
	 * when the dead prefix occupies at least half of the buffer.
	 *
	 * Cost is paid at most once per two message length's of data
	 * regardless of message size or rate, keeping amortized
	 * overhead of O(1) per byte.
	 */
	void maybeCompact() noexcept {
		if (readHead == 0)
			return;

		if (readHead * 2 < recvBuffer.size())
			return;

		const size_t rem = available();
		if (rem > 0)
			std::memmove(recvBuffer.data(), recvBuffer.data() + readHead, rem);

		recvBuffer.resize(rem);
		readHead = 0;
	}
};

} // namespace Blackthorn::Net::Transport::Channels