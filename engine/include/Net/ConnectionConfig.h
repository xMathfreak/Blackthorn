#pragma once

#include <SDL3/SDL.h>

#include "Core/Export.h"
#include "Net/Connection/PeerRateLimiter.h"

namespace Blackthorn::Net {

/**
 * @brief Configuration passed to @c ConnectionManager::start().
 */
struct BLACKTHORN_API ConnectionConfig {
	/// UDP port to bind on (server and client). 0 = OS-assigned ephemeral.
	Uint16 udpPort = 7777;

	/// TCP port to listen on (server only). 0 = disabled.
	Uint16 tcpPort = 7778;

	/// Maximum number of simultaneous peers.
	size_t maxPeers = 64;

	/// Capacity of the inbound packet queue. Must be a power of two.
	size_t queueCapacity = 256;

	/// I/O thread poll interval in microseconds. Default: 500µs.
	Uint32 pollIntervalMicros = 500;

	/// When false, UDP datagrams from unknown addresses are silently dropped.
	bool allowUDPImplicitPeers = true;

	/// Idle time before a TCP peer is probed with a Heartbeat, in ms.
	/// Set to 0 to disable. Default: 5000ms (half the default timeout).
	Uint32 heartbeatIntervalMs = 5000;

	/// Default rate-limit config applied to every new peer.
	Connection::RateLimitConfig rateLimitDefaults = Connection::RateLimitConfig{};
};

} // namespace Blackthorn::Net