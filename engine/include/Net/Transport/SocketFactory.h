#pragma once

#include <memory>

#include "Core/Export.h"
#include "Net/Transport/TCPSocket.h"
#include "Net/Transport/UDPSocket.h"

namespace Blackthorn::Net::Transport {

/**
 * @brief Creates platform socket instances and manages WinSock lifetime.
 *
 * @details On Windows, @c SocketFactory::init() calls @c WSAStartup and
 * @c SocketFactory::shutdown() calls @c WSACleanup. These are no-ops on POSIX
 * systems. Call @c init() once at application startup (before any sockets are
 * created) and @c shutdown() once at exit.
 *
 * @c EngineBase::init() calls @c SocketFactory::init() automatically.
 *
 * @section Usage
 *
 * @code
 * auto udp = SocketFactory::createUDP();
 * udp->setNonBlocking();
 * udp->setReuseAddr();
 * udp->bind(Address::anyIPv4(7777));
 *
 * auto tcp = SocketFactory::createTCP();
 * tcp->setNonBlocking();
 * tcp->setNoDelay();
 * tcp->bind(Address::anyIPv4(7778));
 * tcp->listen();
 * @endcode
 */
class BLACKTHORN_API SocketFactory {
public:
	SocketFactory() = delete;

	/**
	 * @brief Initializes the socket subsystem.
	 *
	 * Must be called before any socket is created. Safe to call multiple
	 * times — only the first call has any effect.
	 *
	 * @return true on success.
	 */
	static bool init();

	/**
	 * @brief Shuts down the socket subsystem.
	 *
	 * Should be called once at application exit, after all sockets are
	 * closed. Safe to call if `init()` was never called.
	 */
	static void shutdown();

	/** @brief Returns true if `init()` has been called successfully. */
	static bool isInitialized();

	/**
	 * @brief Creates a non-blocking UDP socket in non-blocking mode.
	 *
	 * The socket is not yet bound. Call `bind()` before receiving.
	 */
	static std::unique_ptr<UDPSocket> createUDP();

	/**
	 * @brief Creates a TCP socket in non-blocking mode with TCP_NODELAY set.
	 *
	 * The socket is not yet bound or connected.
	 */
	static std::unique_ptr<TCPSocket> createTCP();

private:
	static inline bool initialized = false;
};

} // namespace Blackthorn::Net::Transport