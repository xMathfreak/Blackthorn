#include "Net/Transport/UDPSocket.h"

#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#define SHUT_RDWR SD_BOTH
#else
	#include <cerrno>
	#include <cstring>
	#include <fcntl.h>
	#include <netinet/in.h>
	#include <sys/socket.h>
	#include <unistd.h>
#endif

#include "Debug/Logger.h"

namespace Blackthorn::Net::Transport {

UDPSocket::UDPSocket() {

}

UDPSocket::~UDPSocket() {
	close();
}

bool UDPSocket::openIPv4() {
	fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (fd == INVALID_SOCKET_HANDLE) {
		BT_ERROR("UDPSocket: failed to create IPv4 socket: {}", platformError());
		return false;
	}
	setNonBlocking(true);
	return true;
}

bool UDPSocket::openIPv6() {
	fd = ::socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if (fd == INVALID_SOCKET_HANDLE) {
		BT_ERROR("UDPSocket: failed to create IPv6 socket: {}", platformError());
		return false;
	}

	int off = 0;
	::setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY,
		reinterpret_cast<const char*>(&off), sizeof(off));
	setNonBlocking(true);
	return true;
}

std::string UDPSocket::platformError() {
	#ifdef _WIN32
		char buf[256]{};
		FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, WSAGetLastError(),
			0, buf, sizeof(buf), nullptr);
		return buf;
	#else
		return std::strerror(errno);
	#endif
}

bool UDPSocket::bind(const Address& address) {
	if (fd == INVALID_SOCKET_HANDLE) {
		bool ok = (address.version() == IPVersion::IPv6)
			? openIPv6()
			: openIPv4();

		if (!ok)
			return false;

		setReuseAddr(true);
	}

	if (::bind(fd, address.raw(), address.rawSize()) != 0) {
		BT_ERROR("UDPSocket: bind failed on {}: {}", address.toString(), platformError());
		return false;
	}

	BT_DEBUG("UDPSocket: bound to {}", address.toString());
	return true;
}

SocketResult UDPSocket::sendTo(const void* data, size_t size, const Address& address) {
	if (fd == INVALID_SOCKET_HANDLE)
		return SocketResult::Error;

	ssize_t sent = ::sendto(
		fd,
		static_cast<const char*>(data),
		static_cast<int>(size),
		0,
		address.raw(),
		address.rawSize()
	);

	if (sent < 0) {
		#ifdef _WIN32
			int err = WSAGetLastError();
			if (err == WSAEWOULDBLOCK) return SocketResult::WouldBlock;
		#else
			if (errno == EAGAIN || errno == EWOULDBLOCK) return SocketResult::WouldBlock;
		#endif
		BT_WARN("UDPSocket: sendTo failed: {}", platformError());
		return SocketResult::Error;
	}

	return SocketResult::Ok;
}

SocketResult UDPSocket::send(const void* data, size_t size, size_t& outBytesSent) {
	(void)data; (void)size; (void)outBytesSent;
	return SocketResult::Error;
}

SocketResult UDPSocket::recvFrom(
	void* buffer,
	size_t bufferSize,
	size_t& outSize,
	Address& outAddress)
{
	if (fd == INVALID_SOCKET_HANDLE) {
		BT_ERROR("Socket is not open");
		return SocketResult::Error;
	}

	sockaddr_storage srcAddr{};
	socklen_t addrLen = sizeof(srcAddr);

	ssize_t received = ::recvfrom(
		fd,
		static_cast<char*>(buffer),
		static_cast<int>(bufferSize),
		0,
		reinterpret_cast<sockaddr*>(&srcAddr),
		&addrLen
	);

	if (received < 0) {
		#ifdef _WIN32
			int err = WSAGetLastError();

			if (err == WSAEWOULDBLOCK)
				return SocketResult::WouldBlock;
		#else
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return SocketResult::WouldBlock;
		#endif

		return SocketResult::Error;
	}

	outSize = static_cast<size_t>(received);

	if (srcAddr.ss_family == AF_INET) {
		const auto* sa = reinterpret_cast<const sockaddr_in*>(&srcAddr);
		char ipStr[INET_ADDRSTRLEN]{};
		inet_ntop(AF_INET, &sa->sin_addr, ipStr, sizeof(ipStr));
		outAddress = Address::fromIPv4(ipStr, ntohs(sa->sin_port));
	} else if (srcAddr.ss_family == AF_INET6) {
		const auto* sa = reinterpret_cast<const sockaddr_in6*>(&srcAddr);
		char ipStr[INET6_ADDRSTRLEN]{};
		inet_ntop(AF_INET6, &sa->sin6_addr, ipStr, sizeof(ipStr));
		outAddress = Address::fromIPv6(ipStr, ntohs(sa->sin6_port));
	}

	return SocketResult::Ok;
}

SocketResult UDPSocket::recv(void* buffer, size_t bufferSize, size_t& outSize) {
	Address dummy;
	return recvFrom(buffer, bufferSize, outSize, dummy);
}

bool UDPSocket::setNonBlocking(bool enabled) {
	if (fd == INVALID_SOCKET_HANDLE) return false;

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

bool UDPSocket::setReuseAddr(bool enabled) {
	if (fd == INVALID_SOCKET_HANDLE) return false;
	int opt = enabled ? 1 : 0;
	return ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
		reinterpret_cast<const char*>(&opt), sizeof(opt)) == 0;
}

void UDPSocket::close() {
	if (fd != INVALID_SOCKET_HANDLE) {
		#ifdef _WIN32
			::closesocket(fd);
		#else
			::close(fd);
		#endif
		fd = INVALID_SOCKET_HANDLE;
	}
}

Address UDPSocket::getLocalAddress() const {
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

std::string UDPSocket::getLastError() const {
	return platformError();
}

} // namespace Blackthorn::Net::Transport