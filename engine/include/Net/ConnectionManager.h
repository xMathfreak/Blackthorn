#pragma once

#include <vector>

#include "Core/Export.h"
#include "Core/EngineConfig.h"
#include "IO/ByteBuffer.h"
#include "Net/Connection/NetworkPeer.h"
#include "Net/Connection/PeerRateLimiter.h"
#include "Net/Connection/PeerRegistry.h"
#include "Net/ConnectionEventBus.h"
#include "Net/NetworkIOWorker.h"
#include "Net/PacketDispatcher.h"
#include "Net/Transport/Address.h"

namespace Blackthorn {

namespace Jobs {
	class JobSystem;
} // namespace Jobs

namespace Net {
/**
 * @brief Central network coordinator.
 *
 * @details Owns and wires together @c PeerRegistry, @c NetworkIOWorker,
 * @c PacketDispatcher, and @c ConnectionEventBus. Exposes the public API
 * that the rest of the engine uses; the implementation is almost entirely
 * delegation.
 *
 * @par Roles
 *
 * - @b Server: Call @c start() with @c tcpPort > 0.
 * - @b Client: Call @c start() then @c connect().
 *
 * @par Callback threading guarantee
 *
 * @c ConnectHandler and @c DisconnectHandler are always invoked on the
 * simulation thread inside @c poll(). @c PacketHandler is invoked on a
 * worker thread when a @c JobSystem is provided, or synchronously otherwise.
 *
 * @par Sending
 *
 * @c sendUDP(), @c sendTCP(), @c broadcastUDP(), and @c broadcastTCP()
 * are thread-safe and may be called from any thread.
 *
 * @code
 * ConnectionManager cm;
 * cm.onPacket(myPacketHandler);
 * cm.onConnect(myConnectHandler);
 * cm.onDisconnect(myDisconnectHandler);
 * cm.start(cfg);
 *
 * // Each tick on the simulation thread:
 * cm.poll(jobSystem);
 *
 * // Sending (any thread):
 * cm.sendUDP(peerId, buf);
 * cm.sendTCP(peerId, buf);
 * @endcode
 */
class BLACKTHORN_API ConnectionManager {
public:
	ConnectionManager();
	~ConnectionManager();

	ConnectionManager(const ConnectionManager&) = delete;
	ConnectionManager& operator=(const ConnectionManager&) = delete;

	/**
	 * @brief Initialises the peer registry, binds sockets, and starts the
	 * I/O thread.
	 *
	 * @param cfg Configuration.
	 * @return true on success.
	 */
	bool start(const ConnectionConfig& cfg = ConnectionConfig());

	/**
	 * @brief Stops the I/O thread, closes all sockets, and resets all peers.
	 *
	 * Safe to call if @c start() was never called. Called automatically by
	 * the destructor.
	 */
	void stop();

	bool isRunning() const noexcept { return ioWorker.isRunning(); }

	/**
	 * @brief Initiates a connection to a server (client role).
	 *
	 * @param address Server address (IP + TCP port).
	 * @return Assigned PeerId, or @c INVALID_PEER_ID on failure.
	 */
	Connection::PeerId connect(const Transport::Address& address);

	/**
	 * @brief Gracefully disconnects a peer, sending a Disconnect packet
	 * over TCP before closing the socket.
	 */
	void disconnect(Connection::PeerId peerId);

	/** @brief Sends @p payload to @p peerId over UDP. */
	bool sendUDP(Connection::PeerId peerId, const IO::ByteBuffer& payload);

	/** @brief Sends @p payload to @p peerId over TCP. */
	bool sendTCP(Connection::PeerId peerId, const IO::ByteBuffer& payload);

	/** @brief Broadcasts @p payload over UDP to all UDP-connected peers. */
	void broadcastUDP(const IO::ByteBuffer& payload);

	/** @brief Broadcasts @p payload over TCP to all TCP-connected peers. */
	void broadcastTCP(const IO::ByteBuffer& payload);

	/**
	 * @brief Drains the packet queue and event bus, dispatches work.
	 *
	 * Must be called once per tick from the simulation thread.
	 *
	 * @param jobs Optional job system for parallel packet dispatch.
	 */
	void poll(Jobs::JobSystem* jobs) { dispatcher.poll(jobs); }

	void onPacket(PacketHandler h) { dispatcher.onPacket(std::move(h)); }
	void onConnect(ConnectHandler h) { dispatcher.onConnect(std::move(h)); }
	void onDisconnect(DisconnectHandler h) { dispatcher.onDisconnect(std::move(h)); }

	/** @brief Returns a const pointer to a peer by ID, or nullptr. */
	const Connection::NetworkPeer* getPeer(Connection::PeerId id) const {
		return registry.get(id);
	}

	/**
	 * @brief Returns a snapshot copy of all peer slots.
	 *
	 * Safe to iterate after the call returns, on any thread.
	 */
	std::vector<Connection::NetworkPeer> getPeerSnapshot() const {
		return registry.snapshot();
	}

	/** @brief Number of peers currently in the Connected state. */
	size_t connectedPeerCount() const { return registry.connectedCount(); }

	/** @brief Maximum peers this manager was configured for. */
	size_t maxPeers() const noexcept { return registry.capacity(); }

	/** @brief Overrides the rate-limit config for a specific peer. */
	void setPeerRateLimit(
		Connection::PeerId peerId,
		const Connection::RateLimitConfig& config)
	{
		registry.setRateLimit(peerId, config);
	}

private:
	Connection::PeerRegistry registry;
	ConnectionEventBus eventBus;
	NetworkIOWorker ioWorker;
	PacketDispatcher dispatcher;
};

} // namespace Net
} // namespace Blackthorn