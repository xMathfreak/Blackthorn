#include "Net/Transport/Sockets/TCPSocket.h"

#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <mstcpip.h>
	#define SHUT_RDWR SD_BOTH
#else
	#include <cerrno>
	#include <cstring>
	#include <fcntl.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <sys/socket.h>
	#include <unistd.h>
#endif

#include "Debug/Logger.h"

namespace Blackthorn::Net::Transport::Sockets {

TCPSocket::TCPSocket() {}

TCPSocket::TCPSocket(SocketHandle existingFd)
	: fd(existingFd)
	, connected(existingFd != INVALID_SOCKET_HANDLE)
{
	if (fd != INVALID_SOCKET_HANDLE) {
		setNoDelay();
		setNonBlocking();
	}
}

TCPSocket::~TCPSocket() {
	close();
}

bool TCPSocket::openIPv4() {
	fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (fd == INVALID_SOCKET_HANDLE) {
		BT_ERROR("TCPSocket: failed to create IPv4 socket: {}", platformError());
		return false;
	}
	return true;
}

bool TCPSocket::openIPv6() {
	fd = ::socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	if (fd == INVALID_SOCKET_HANDLE) {
		BT_ERROR("TCPSocket: failed to create IPv6 socket: {}", platformError());
		return false;
	}
	int off = 0;
	::setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY,
		reinterpret_cast<const char*>(&off), sizeof(off));
	return true;
}

std::string TCPSocket::platformError() {
#ifdef _WIN32
	char buf[256]{};
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, WSAGetLastError(),
		0, buf, sizeof(buf), nullptr);
	return buf;
#else
	return std::strerror(errno);
#endif
}

bool TCPSocket::bind(const Address& address) {
	if (fd == INVALID_SOCKET_HANDLE) {
		bool ok = (address.version() == IPVersion::IPv6) ? openIPv6() : openIPv4();
		if (!ok)
			return false;

		setReuseAddr();
	}

	setNonBlocking();
	setNoDelay();

	if (::bind(fd, address.raw(), address.rawSize()) != 0) {
		BT_ERROR("TCPSocket: bind failed on {}: {}", address.toString(), platformError());
		return false;
	}
	return true;
}

bool TCPSocket::connect(const Address& address) {
	if (fd == INVALID_SOCKET_HANDLE) {
		bool ok = (address.version() == IPVersion::IPv6) ? openIPv6() : openIPv4();
		if (!ok) return false;
	}

	setNonBlocking();

	int result = ::connect(fd, address.raw(), address.rawSize());

#ifdef _WIN32
	if (result == SOCKET_ERROR) {
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK || err == WSAEINPROGRESS) {
			// Non-blocking connect in progress — poll isConnected().
			return true;
		}
		BT_ERROR("TCPSocket: connect failed: {}", platformError());
		return false;
	}
#else
	if (result < 0) {
		if (errno == EINPROGRESS) {
			return true;
		}
		BT_ERROR("TCPSocket: connect failed: {}", platformError());
		return false;
	}
#endif

	connected = true;
	return true;
}

bool TCPSocket::listen(int backlog) {
	if (fd == INVALID_SOCKET_HANDLE) return false;
	return ::listen(fd, backlog) == 0;
}

std::unique_ptr<ISocket> TCPSocket::accept(Address& outAddress) {
	if (fd == INVALID_SOCKET_HANDLE) return nullptr;

	sockaddr_storage clientAddr{};
	socklen_t addrLen = sizeof(clientAddr);

	SocketHandle clientFd = ::accept(fd, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
	if (clientFd == INVALID_SOCKET_HANDLE) {
#ifdef _WIN32
		if (WSAGetLastError() == WSAEWOULDBLOCK) return nullptr;
#else
		if (errno == EAGAIN || errno == EWOULDBLOCK) return nullptr;
#endif
		BT_ERROR("TCPSocket: accept failed: {}", platformError());
		return nullptr;
	}

	// Set non-blocking immediately for the client
	auto sock = std::make_unique<TCPSocket>(clientFd);
	sock->setNoDelay();
	sock->setNonBlocking(); // client socket only

	// Fill out the address
	if (clientAddr.ss_family == AF_INET) {
		const auto* sa = reinterpret_cast<const sockaddr_in*>(&clientAddr);
		char ip[INET_ADDRSTRLEN]{};
		inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));
		outAddress = Address::fromIPv4(ip, ntohs(sa->sin_port));
	} else {
		const auto* sa = reinterpret_cast<const sockaddr_in6*>(&clientAddr);
		char ip[INET6_ADDRSTRLEN]{};
		inet_ntop(AF_INET6, &sa->sin6_addr, ip, sizeof(ip));
		outAddress = Address::fromIPv6(ip, ntohs(sa->sin6_port));
	}

	return sock;
}

SocketResult TCPSocket::sendTo(const void* data, size_t size, const Address&) {
	size_t sent = 0;
	return send(data, size, sent);
}

SocketResult TCPSocket::send(const void* data, size_t size, size_t& outBytesSent) {
	outBytesSent = 0;

	if (fd == INVALID_SOCKET_HANDLE)
		return SocketResult::Error;

	ssize_t sent = ::send(
		fd,
		static_cast<const char*>(data),
		static_cast<int>(size),
		0
	);

	if (sent < 0) {
#ifdef _WIN32
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK) return SocketResult::WouldBlock;
		if (err == WSAECONNRESET || err == WSAECONNABORTED)
			return SocketResult::Disconnected;
#else
		if (errno == EAGAIN || errno == EWOULDBLOCK) return SocketResult::WouldBlock;
		if (errno == EPIPE || errno == ECONNRESET) return SocketResult::Disconnected;
#endif
		return SocketResult::Error;
	}

	outBytesSent = static_cast<size_t>(sent);

	return SocketResult::Ok;
}

SocketResult TCPSocket::recvFrom(
	void*buffer,
	size_t bufferSize,
	size_t&outSize,
	Address&)
{
	return recv(buffer, bufferSize, outSize);
}

SocketResult TCPSocket::recv(void* buffer, size_t bufferSize, size_t& outSize) {
	if (fd == INVALID_SOCKET_HANDLE)
		return SocketResult::Error;

	ssize_t received;

	do {
		received = ::recv(
			fd,
			static_cast<char*>(buffer),
			static_cast<int>(bufferSize),
			0
		);

	#ifdef _WIN32
		int err = WSAGetLastError();
		if (received < 0 && err == WSAEWOULDBLOCK)
			return SocketResult::WouldBlock;
	#else
		if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			return SocketResult::WouldBlock;
	#endif

	} while (false);

	if (received == 0) {
		connected = false;
		return SocketResult::Disconnected;
	}

	if (received < 0) {
#ifdef _WIN32
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK)
			return SocketResult::WouldBlock;
#else
		if (errno == EAGAIN || errno == EWOULDBLOCK) return SocketResult::WouldBlock;
#endif
		return SocketResult::Error;
	}

	outSize = static_cast<size_t>(received);
	return SocketResult::Ok;
}

bool TCPSocket::setNonBlocking(bool enabled) {
	if (fd == INVALID_SOCKET_HANDLE)
		return false;

#ifdef _WIN32
	u_long mode = enabled ? 1 : 0;
	return ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) return false;
	flags = enabled ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
	return fcntl(fd, F_SETFL, flags) == 0;
#endif
}

bool TCPSocket::setReuseAddr(bool enabled) {
	if (fd == INVALID_SOCKET_HANDLE) return false;
	int opt = enabled ? 1 : 0;
	return ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
		reinterpret_cast<const char*>(&opt), sizeof(opt)) == 0;
}

bool TCPSocket::setNoDelay(bool enabled) {
	if (fd == INVALID_SOCKET_HANDLE) return false;
	int opt = enabled ? 1 : 0;
	return ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
		reinterpret_cast<const char*>(&opt), sizeof(opt)) == 0;
}

void TCPSocket::close() {
	if (fd != INVALID_SOCKET_HANDLE) {
#ifdef _WIN32
		::closesocket(fd);
#else
		::close(fd);
#endif
		fd= INVALID_SOCKET_HANDLE;
		connected = false;
	}
}

Address TCPSocket::getLocalAddress() const {
	if (fd == INVALID_SOCKET_HANDLE) return {};
	sockaddr_storage sa{};
	socklen_t len = sizeof(sa);
	if (::getsockname(fd, reinterpret_cast<sockaddr*>(&sa), &len) != 0)
		return {};

	if (sa.ss_family == AF_INET) {
		const auto* s = reinterpret_cast<const sockaddr_in*>(&sa);
		char ip[INET_ADDRSTRLEN]{};
		inet_ntop(AF_INET, &s->sin_addr, ip, sizeof(ip));
		return Address::fromIPv4(ip, ntohs(s->sin_port));
	}

	const auto* s = reinterpret_cast<const sockaddr_in6*>(&sa);
	char ip[INET6_ADDRSTRLEN]{};
	inet_ntop(AF_INET6, &s->sin6_addr, ip, sizeof(ip));
	return Address::fromIPv6(ip, ntohs(s->sin6_port));
}

bool TCPSocket::isConnected() const {
	if (fd == INVALID_SOCKET_HANDLE)
		return false;

	if (connected)
		return true;

	int err = 0;
	socklen_t errLen = sizeof(err);
	if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &errLen) != 0)
		return false;

	if (err != 0)
		return false;

	connected = true;
	return true;
}

std::string TCPSocket::getLastError() const {
	return platformError();
}

} // namespace Blackthorn::Net::Transport::Sockets