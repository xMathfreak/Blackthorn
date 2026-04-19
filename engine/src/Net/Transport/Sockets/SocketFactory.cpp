#include "Net/Transport/Sockets/SocketFactory.h"

#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
#else
	#include <arpa/inet.h>
	#include <netdb.h>
#endif

#include "Debug/Logger.h"

namespace Blackthorn::Net::Transport::Sockets {

bool SocketFactory::init() {
	if (initialized)
		return true;

#ifdef _WIN32
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		BT_ERROR("SocketFactory: WSAStartup failed");
		return false;
	}

	BT_DEBUG(
		"SocketFactory: WinSock2 initialized (v{}.{})",
		LOBYTE(wsaData.wVersion), HIBYTE(wsaData.wVersion)
	);
#endif

	initialized = true;
	return true;
}

void SocketFactory::shutdown() {
	if (!initialized)
		return;

#ifdef _WIN32
	WSACleanup();
	BT_DEBUG("SocketFactory: WinSock2 cleaned up");
#endif

	initialized = false;
}

bool SocketFactory::isInitialized() {
	return initialized;
}

std::unique_ptr<UDPSocket> SocketFactory::createUDP() {
	if (!initialized) {
		BT_WARN("SocketFactory: createUDP called before init()");
		return nullptr;
	}

	auto sock = std::make_unique<UDPSocket>();
	sock->setNonBlocking(true);
	return sock;
}

std::unique_ptr<TCPSocket> SocketFactory::createTCP() {
	if (!initialized) {
		BT_WARN("SocketFactory: createTCP called before init()");
		return nullptr;
	}

	auto sock = std::make_unique<TCPSocket>();
	sock->setNonBlocking(true);
	sock->setNoDelay(true);
	return sock;
}

} // namespace Blackthorn::Net::Transport::Sockets