#pragma once

#include <mutex>
#include <vector>


#include "Core/Export.h"
#include "Core/Types/Numeric.h"
#include "Net/Connection/NetworkPeer.h"
#include "Net/Transport/Address.h"

namespace Blackthorn::Net {

/**
 * @brief Type of a connection lifecycle event.
 */
enum class ConnectionEventType : U8 {
	Connect, ///< A peer completed its handshake and is now Connected.
	Disconnect, ///< A peer timed out, was rate-kicked, or explicitly disconnected.
};

/**
 * @brief A single queued connection lifecycle event.
 */
struct ConnectionEvent {
	ConnectionEventType type;
	Connection::PeerId peerId = Connection::INVALID_PEER_ID;
	Transport::Address address; ///< Populated for Connect; empty for Disconnect.
};

/**
 * @brief Thread-safe MPSC queue for connection lifecycle events.
 *
 * @details The I/O thread pushes events via @c push() as peers connect,
 * disconnect, or are rate-kicked. The simulation thread drains the queue
 * in @c PacketDispatcher::poll() via @c drainInto(), then fires the
 * registered callbacks without holding the lock.
 *
 * @par Lock ordering
 *
 * @c ConnectionEventBus acquires only @c eventMutex. It must never be
 * called while @c PeerRegistry::mutex() is held, to prevent a lock-order
 * inversion between the I/O thread (which holds peerMutex then pushes
 * events) and the simulation thread (which dispatches events then sends,
 * acquiring peerMutex).
 */
class BLACKTHORN_API ConnectionEventBus {
public:
	/**
	 * @brief Appends an event to the queue.
	 *
	 * Thread-safe. May be called from any thread. Must not be called
	 * while @c PeerRegistry::mutex() is held.
	 */
	void push(ConnectionEvent event) {
		std::lock_guard<std::mutex> lock(eventMutex);
		pending.push_back(std::move(event));
	}

	/**
	 * @brief Swaps the pending queue into @p out under the lock, leaving
	 * the internal queue empty.
	 *
	 * Callers fire the callbacks after this returns, without holding any
	 * lock, so handlers may freely call @c push() or acquire @c peerMutex.
	 *
	 * @param out Vector to receive the events. Existing contents are cleared.
	 */
	void drainInto(std::vector<ConnectionEvent>& out) {
		out.clear();
		std::lock_guard<std::mutex> lock(eventMutex);
		out.swap(pending);
	}

	/** @brief Discards all queued events without firing them. */
	void clear() {
		std::lock_guard<std::mutex> lock(eventMutex);
		pending.clear();
	}

private:
	std::vector<ConnectionEvent> pending;
	mutable std::mutex eventMutex;
};

} // namespace Blackthorn::Net