#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL.h>

#include "Core/Export.h"
#include "Jobs/JobSystem.h"
#include "Net/ByteBuffer.h"
#include "Net/PacketHeader.h"
#include "Net/Transport/Address.h"
#include "Net/Transport/NetworkPeer.h"
#include "Net/Transport/PacketQueue.h"
#include "Net/Transport/UDPSocket.h"
#include "Net/Transport/TCPSocket.h"

namespace Blackthorn::Net::Transport {

/**
 * @brief Callback invoked on the simulation thread for each received packet.
 *
 * @param peerId  The peer that sent the packet.
 * @param header  Deserialized PacketHeader (already validated).
 * @param payload ByteBuffer positioned at the start of the payload
 *                (after the PacketHeader bytes).
 */
using PacketHandler = std::function<void(
	PeerId peerId,
	const PacketHeader& header,
	Net::ByteBuffer& payload
)>;

/**
 * @brief Callback invoked on the simulation thread when a peer connects.
 * @param peerId  The newly connected peer.
 * @param address Remote address of the peer.
 */
using ConnectHandler = std::function<void(PeerId, const Address&)>;

/**
 * @brief Callback invoked on the simulation thread when a peer disconnects.
 * @param peerId  The peer that disconnected.
 */
using DisconnectHandler = std::function<void(PeerId)>;

/**
 * @brief Configuration passed to `ConnectionManager::start()`.
 */
struct ConnectionConfig {
	/// UDP port to bind on (server and client). 0 = OS-assigned ephemeral.
	Uint16 udpPort = 7777;

	/// TCP port to listen on (server only). 0 = disabled.
	Uint16 tcpPort = 7778;

	/// Maximum number of simultaneous peers.
	size_t maxPeers = 64;

	/// Capacity of the inbound packet queue. Must be power of two.
	/// Default matches DefaultPacketQueue.
	size_t queueCapacity = 256;

	/// Period between I/O thread poll iterations in microseconds.
	/// Lower = less latency, higher CPU. Default: 500µs (2000 polls/sec).
	Uint32 pollIntervalMicros = 500;

	/// Block unknown UDP senders
	bool allowUDPImplicitPeers = true;
};

enum class ConnectionEventType : Uint8 {
	Connect,
	Disconnect
};

struct ConnectionEvent {
	ConnectionEventType type;
	PeerId peerId;
	Address address;
};

/**
 * @brief Central connection manager.
 *
 * @details Owns all @c NetworkPeer objects, the server UDP socket, the TCP
 * listen socket, and the dedicated I/O thread. Bridges the I/O thread and the
 * simulation thread via a lock-free @c PacketQueue.
 *
 * @par Roles
 *
 * - @b Server: Call @c start() with @c tcpPort > 0. The manager binds both a
 *   UDP socket and a TCP listen socket, accepts incoming TCP connections, and
 *   assigns peer IDs to new UDP senders.
 *
 * - @b Client: Call @c start() then @c connect(). The manager binds a local
 *   UDP socket on an ephemeral port and opens a TCP connection to the server
 *   address.
 *
 * @par Dispatch
 *
 * The I/O thread receives packets and pushes @c InboundPacket entries to the
 * queue. The simulation thread calls @c poll() each tick, which drains the
 * queue and dispatches each packet as a @c JobSystem task via the registered
 * @c PacketHandler. This keeps packet processing parallel with the rest of
 * the tick.
 *
 * @par Sending
 *
 * @c sendUDP() and @c sendTCP() may be called from any thread. Internally,
 * they take a mutex on the outbound socket. High-frequency snapshot sends
 * should prefer @c sendUDP().
 *
 * @code
 * ConnectionManager cm;
 * cm.onPacket(myPacketHandler);
 * cm.onConnect(myConnectHandler);
 * cm.onDisconnect(myDisconnectHandler);
 * cm.start(cfg);
 *
 * // Server tick:
 * cm.poll(jobSystem);  // called once per tick in EngineCore::update()
 *
 * // Sending:
 * cm.sendUDP(peerId, packetBuf);
 * cm.sendTCP(peerId, messageBuf);
 * @endcode
 */
class BLACKTHORN_API ConnectionManager {
public:
	explicit ConnectionManager() = default;
	~ConnectionManager();

	ConnectionManager(const ConnectionManager&) = delete;
	ConnectionManager& operator=(const ConnectionManager&) = delete;

	/**
	 * @brief Starts the connection manager and the I/O thread.
	 *
	 * Binds the UDP socket, optionally binds and listens on the TCP socket,
	 * and launches the I/O thread.
	 *
	 * @param cfg Configuration (ports, max peers, poll interval).
	 * @return true on success.
	 */
	bool start(const ConnectionConfig& cfg = ConnectionConfig{});

	/**
	 * @brief Stops the I/O thread, closes all sockets, and clears all peers.
	 *
	 * Safe to call if `start()` was never called. Called automatically by
	 * the destructor.
	 */
	void stop();

	bool isRunning() const noexcept { return ioRunning.load(std::memory_order::relaxed); }

	/**
	 * @brief Initiates a connection to a server (client role).
	 *
	 * Opens a TCP connection to `address.port()` and registers a peer slot.
	 * UDP traffic is sent to the same IP and `cfg.udpPort`.
	 *
	 * @param address Server address (IP + TCP port).
	 * @return The assigned PeerId, or INVALID_PEER_ID on failure.
	 */
	PeerId connect(const Address& address);

	/**
	 * @brief Disconnects a peer gracefully, sending a Disconnect packet
	 * before closing the TCP socket.
	 *
	 * @param peerId Peer to disconnect.
	 */
	void disconnect(PeerId peerId);

	/**
	 * @brief Sends `payload` to `peerId` over UDP.
	 *
	 * Stamps the UDPChannel sequence number and ACK state before sending.
	 * Thread-safe.
	 *
	 * @param peerId  Destination peer.
	 * @param payload ByteBuffer beginning with a serialized PacketHeader.
	 * @return true if the packet was sent successfully.
	 */
	bool sendUDP(PeerId peerId, const Net::ByteBuffer& payload);

	/**
	 * @brief Sends `payload` to `peerId` over TCP with length-prefix framing.
	 * Thread-safe.
	 *
	 * @param peerId  Destination peer.
	 * @param payload ByteBuffer beginning with a serialized PacketHeader.
	 * @return true if the message was sent successfully.
	 */
	bool sendTCP(PeerId peerId, const Net::ByteBuffer& payload);

	/**
	 * @brief Broadcasts `payload` over UDP to all connected peers.
	 * Thread-safe.
	 */
	void broadcastUDP(const Net::ByteBuffer& payload);

	/**
	 * @brief Broadcasts `payload` over TCP to all connected peers.
	 * Thread-safe.
	 */
	void broadcastTCP(const Net::ByteBuffer& payload);

	/**
	 * @brief Drains the inbound packet queue and dispatches each packet as
	 * a `JobSystem` job.
	 *
	 * Call once per tick from the simulation thread (inside `update()` or
	 * `fixedUpdate()`). Each packet is validated (magic + schema version),
	 * then the registered `PacketHandler` is called inside a submitted job.
	 *
	 * Timed-out peers are detected and the `DisconnectHandler` is invoked
	 * synchronously (not as a job) so the scene can react within the same
	 * tick.
	 *
	 * @param jobs JobSystem to dispatch packet-handling jobs onto.
	 *             If nullptr, packets are handled synchronously.
	 */
	void poll(Jobs::JobSystem* jobs);

	void onPacket(PacketHandler handler) { packetHandler = std::move(handler); }
	void onConnect(ConnectHandler handler) { connectHandler = std::move(handler); }
	void onDisconnect(DisconnectHandler handler) { disconnectHandler = std::move(handler); }

	/** @brief Returns a const pointer to a peer by ID, or nullptr. */
	const NetworkPeer* getPeer(PeerId id) const;

	auto& getAllPeers() const { return peers; }

	/** @brief Returns the number of currently connected peers. */
	size_t connectedPeerCount() const;

	/** @brief Returns the maximum number of peers this manager supports. */
	size_t maxPeers() const noexcept { return peers.size(); }

private:
	void ioThreadLoop();
	void pollUDP();
	void pollTCP();
	void pollTCPAccept();
	void checkTimeouts();

	PeerId findPeer(const Address& address, bool tcp);
	PeerId findOrCreatePeer(const Address& address, bool tcp);
	PeerId allocatePeerSlot(const Address& address, bool tcp);
	void freePeerSlot(PeerId id, bool tcp);

	ConnectionConfig cfg;

	std::vector<NetworkPeer> peers;
	std::unordered_map<Address, PeerId> addressToPeerTCP;
	std::unordered_map<Address, PeerId> addressToPeerUDP;
	mutable std::mutex peerMutex;

	std::unique_ptr<UDPSocket> udpSocket;
	std::unique_ptr<TCPSocket> tcpListenSocket;
	mutable std::mutex sendMutex;

	std::vector<ConnectionEvent> pendingEvents;
	mutable std::mutex eventMutex;

	DefaultPacketQueue inboundQueue;

	std::thread ioThread;
	std::atomic<bool> ioRunning { false };

	PacketHandler packetHandler;
	ConnectHandler connectHandler;
	DisconnectHandler disconnectHandler;

	// Scratch buffer used by the I/O thread for receives — avoids per-packet
	// allocation. Only accessed from the I/O thread.
	static constexpr size_t RECV_BUFFER_SIZE = 65536;
	std::vector<Uint8> recvScratch;
};

} // namespace Blackthorn::Net::Transport