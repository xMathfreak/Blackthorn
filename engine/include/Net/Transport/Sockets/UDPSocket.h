#pragma once

#include "Core/Export.h"
#include "Net/Transport/Sockets/ISocket.h"
#include "Net/Transport/Sockets/SocketHandle.h"

namespace Blackthorn::Net::Transport::Sockets {

/**
 * @brief Platform UDP socket.
 *
 * @details Wraps a raw @c SOCKET (Windows) or @c int (POSIX) file descriptor
 * behind the @c ISocket interface. Created via
 * @c SocketFactory::createUDP().
 *
 * @par Dual-stack
 *
 * When binding to an IPv6 any-address, the @c IPV6_V6ONLY option is set to
 * @c 0, enabling IPv4-mapped IPv6 addresses so a single socket can receive
 * both IPv4 and IPv6 datagrams.
 *
 * When binding to an explicit IPv4 address, a plain @c AF_INET socket is used
 * instead.
 */
class BLACKTHORN_API UDPSocket : public ISocket {
public:
	UDPSocket();
	~UDPSocket() override;

	UDPSocket(const UDPSocket&) = delete;
	UDPSocket& operator=(const UDPSocket&) = delete;

	bool bind(const Transport::Address& address) override;

	/// Not applicable to UDP — always returns false.
	bool connect(const Transport::Address&) override { return false; }

	/// Not applicable to UDP — always returns false.
	bool listen(int) override { return false; }

	/// Not applicable to UDP — always returns nullptr.
	std::unique_ptr<ISocket> accept(Transport::Address&) override { return nullptr; }

	SocketResult sendTo(
		const void* data,
		size_t size,
		const Transport::Address& address) override;

	/// Equivalent to sendTo with the last address passed to bind().
	/// Not useful for connectionless UDP — prefer sendTo.
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
	void close() override;

	bool isOpen() const override { return fd != INVALID_SOCKET_HANDLE; }
	bool isConnected() const override { return false; }

	Transport::Address getLocalAddress() const override;
	std::string getLastError() const override;

	/** @brief Returns the raw socket handle. Used by ConnectionManager::select(). */
	SocketHandle handle() const { return fd; }

private:
	SocketHandle fd = INVALID_SOCKET_HANDLE;

	bool openIPv4();
	bool openIPv6();

	static std::string platformError();
};

} // namespace Blackthorn::Net::Transport::Sockets