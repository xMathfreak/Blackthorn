#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include "Core/Export.h"
#include "Core/Types/Numeric.h"
#include "Net/ConnectionConfig.h"
#include "Net/ConnectionEventBus.h"
#include "Net/Transport/PacketQueue.h"
#include "Net/Transport/Sockets/TCPSocket.h"
#include "Net/Transport/Sockets/UDPSocket.h"

namespace Blackthorn::Net {

namespace Connection {
	class PeerRegistry;
} // namespace Connection

/**
 * @brief Owns the I/O thread, both sockets, and all socket polling logic.
 *
 * @details @c NetworkIOWorker runs a dedicated background thread that
 * continuously polls the UDP socket and all active TCP connections for
 * incoming data. Received packets are pushed into the shared
 * @c inboundQueue for consumption by @c PacketDispatcher::poll() on the
 * simulation thread. Connection lifecycle events (connect, disconnect,
 * rate-kick) are pushed into @c ConnectionEventBus.
 *
 * @par Ownership
 *
 * @c NetworkIOWorker owns @c udpSocket, @c tcpListenSocket, @c ioThread,
 * and the @c recvScratch buffer. It holds non-owning references to
 * @c PeerRegistry, @c ConnectionEventBus, and the shared @c inboundQueue
 * - all of which are owned by @c ConnectionManager and outlive the worker.
 *
 * @par Thread safety
 *
 * All methods except @c start() and @c stop() are intended to be called
 * only from the I/O thread. @c start() and @c stop() are called from the
 * simulation thread during engine lifecycle transitions.
 */
class BLACKTHORN_API NetworkIOWorker {
public:
	NetworkIOWorker() = default;
	~NetworkIOWorker() { stop(); }

	NetworkIOWorker(const NetworkIOWorker&) = delete;
	NetworkIOWorker& operator=(const NetworkIOWorker&) = delete;

	/**
	 * @brief Binds sockets, starts the I/O thread.
	 *
	 * @param config Network configuration (ports, poll interval, etc.).
	 * @param registry Peer registry shared with the rest of the system.
	 * @param eventBus Event queue for lifecycle notifications.
	 * @param inboundQueue Packet queue drained by @c PacketDispatcher.
	 * @return true on success.
	 */
	bool start(
		const ConnectionConfig& config,
		Connection::PeerRegistry& registry,
		ConnectionEventBus& eventBus,
		Transport::DefaultPacketQueue& inboundQueue
	);

	/**
	 * @brief Signals the I/O thread to stop and joins it.
	 *
	 * Safe to call if @c start() was never called.
	 */
	void stop();

	bool isRunning() const noexcept {
		return ioRunning.load(std::memory_order::relaxed);
	}

	/** @brief Returns the bound UDP socket, or nullptr if not started. */
	Transport::Sockets::UDPSocket* udpSocketPtr() const noexcept {
		return udpSocket.get();
	}

	size_t& getGlobalFragmentBytes() { return globalFragmentBytes; }

private:
	void ioThreadLoop();
	void pollUDP();
	void pollTCP();
	void pollTCPAccept();
	void sendHeartbeats();

	Connection::PeerRegistry* registry = nullptr;
	ConnectionEventBus* eventBus = nullptr;
	Transport::DefaultPacketQueue* inboundQueue = nullptr;

	// Owned resources.
	std::unique_ptr<Transport::Sockets::UDPSocket> udpSocket;
	std::unique_ptr<Transport::Sockets::TCPSocket> tcpListenSocket;

	std::thread ioThread;
	std::atomic<bool> ioRunning { false };

	ConnectionConfig cfg;

	/// Engine-wide in-flight reassembly byte counter shared across
	/// all peer @c FragmentAssembler instances. Enforces the 16 MB
	/// global cap.
	size_t globalFragmentBytes = 0;

	static constexpr size_t RECV_BUFFER_SIZE = 65536;
	std::vector<U8> recvScratch;
};

} // namespace Blackthorn::Net