#pragma once

#include "Core/Export.h"
#include "Net/Transport/Sockets/ISocket.h"
#include "Net/Transport/Sockets/SocketHandle.h"

namespace Blackthorn::Net::Transport::Sockets {

/**
 * @brief Platform TCP socket.
 *
 * @details Wraps a raw stream socket file descriptor behind the
 * @c ISocket interface. Created via @c SocketFactory::createTCP()
 * or returned by @c accept().
 *
 * @par Framing
 *
 * Raw TCP is a byte stream with no message boundaries. Framing
 * (the 4-byte length prefix) is handled by @c TCPChannel, not here.
 * This class only provides raw @c send() / @c recv() semantics.
 */
class BLACKTHORN_API TCPSocket : public ISocket {
public:
	TCPSocket();

	/**
	 * @brief Constructs a TCPSocket from an already-open file descriptor.
	 *
	 * Used by `accept()` to wrap the OS-returned socket handle.
	 */
	explicit TCPSocket(SocketHandle existingFd);

	~TCPSocket() override;

	TCPSocket(const TCPSocket&) = delete;
	TCPSocket& operator=(const TCPSocket&) = delete;

	bool bind(const Transport::Address& address) override;
	bool connect(const Transport::Address& address) override;
	bool listen(int backlog = 8) override;
	std::unique_ptr<ISocket> accept(Transport::Address& outAddress) override;

	SocketResult sendTo(
		const void* data,
		size_t size,
		const Transport::Address& address) override;

	SocketResult send(const void* data, size_t size, size_t& outBytesSent) override;

	SocketResult recvFrom(
		void* buffer,
		size_t bufferSize,
		size_t& outSize,
		Transport::Address& outAddress) override;

	SocketResult recv(
		void* buffer,
		size_t bufferSize,
		size_t& outSize) override;

	bool setNonBlocking(bool enabled = true) override;
	bool setReuseAddr(bool enabled = true) override;

	/** @brief Disables Nagle's algorithm for lower latency on small messages. */
	bool setNoDelay(bool enabled = true);

	void close() override;

	bool isOpen() const override { return fd != INVALID_SOCKET_HANDLE; }
	bool isConnected() const override;

	Transport::Address getLocalAddress() const override;
	std::string getLastError() const override;

	SocketHandle handle() const { return fd; }

private:
	SocketHandle fd = INVALID_SOCKET_HANDLE;
	bool connected = false;

	bool openIPv4();
	bool openIPv6();

	static std::string platformError();
};

} // namespace Blackthorn::Net::Transport::Sockets