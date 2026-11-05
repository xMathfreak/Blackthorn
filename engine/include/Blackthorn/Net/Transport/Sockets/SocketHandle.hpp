#pragma once

#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>

	using SocketHandle = SOCKET;
	static constexpr SocketHandle INVALID_SOCKET_HANDLE = INVALID_SOCKET;
#else
	#include <sys/socket.h>

	using SocketHandle = int;
	static constexpr SocketHandle INVALID_SOCKET_HANDLE = -1;
#endif