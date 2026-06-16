#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "Core/Export.h"
#include "Jobs/JobHandle.h"
#include "Net/ConnectionEventBus.h"
#include "Net/Transport/PacketQueue.h"

namespace Blackthorn {

namespace Jobs {
	class JobSystem;
} // namespace Jobs

namespace Net {

namespace Connection {
	class PeerRegistry;
} // namespace Connection

using PacketHandler = std::function<void(
	Connection::PeerId,
	const Protocol::PacketHeader&,
	IO::ByteBuffer&
)>;

using ConnectHandler = std::function<void(
	Connection::PeerId,
	const Transport::Address&
)>;

using DisconnectHandler = std::function<void(
	Connection::PeerId
)>;

/**
 * @brief Drains the inbound packet queue and the connection event bus on
 * the simulation thread.
 *
 * @details @c PacketDispatcher::poll() is the only entry point for
 * simulation-thread network work. It:
 *
 *  1. Waits on the previous tick's job handle (fence).
 *  2. Detects timed-out peers via @c PeerRegistry.
 *  3. Drains @c ConnectionEventBus and fires @c ConnectHandler /
 *     @c DisconnectHandler on the simulation thread.
 *  4. Drains @c inboundQueue, validates each packet, and dispatches
 *     @c PacketHandler either synchronously or as a @c JobSystem job.
 *
 * @par Callback threading guarantee
 *
 * @c ConnectHandler, @c DisconnectHandler, and (when @p jobs is nullptr)
 * @c PacketHandler are always called on the simulation thread inside
 * @c poll(). When @p jobs is non-null, @c PacketHandler is called on a
 * worker thread - callers must ensure the handler is safe to invoke
 * concurrently with other simulation work that runs between two @c poll()
 * calls.
 */
class BLACKTHORN_API PacketDispatcher {
public:
	PacketDispatcher() = default;

	PacketDispatcher(const PacketDispatcher&) = delete;
	PacketDispatcher& operator=(const PacketDispatcher&) = delete;

	/**
	 * @brief Drains the inbound packet queue and the event bus.
	 *
	 * Must be called once per tick from the simulation thread.
	 *
	 * @param jobs Optional job system. If non-null, packet handler
	 *             invocations are submitted as jobs and fenced at the
	 *             start of the next call.
	 */
	void poll(Jobs::JobSystem* jobs);

	/** @brief Discards the pending job handle on shutdown. */
	void reset() { pendingJobHandle = nullptr; }

	void onPacket(PacketHandler h) { packetHandler = std::move(h); }
	void onConnect(ConnectHandler h) { connectHandler = std::move(h); }
	void onDisconnect(DisconnectHandler h) { disconnectHandler = std::move(h); }

	/** @brief Inbound packet queue - written by @c NetworkIOWorker. */
	Transport::DefaultPacketQueue inboundQueue;

	Connection::PeerRegistry* registry = nullptr;
	ConnectionEventBus* eventBus = nullptr;

private:
	void checkTimeouts();
	void dispatchEvents();

	PacketHandler packetHandler;
	ConnectHandler connectHandler;
	DisconnectHandler disconnectHandler;

	std::vector<ConnectionEvent> eventScratch;

	Jobs::JobHandlePtr pendingJobHandle;

	U32 timeoutCheckCounter = 0;
};

} // namespace Net

} // namespace Blackthorn