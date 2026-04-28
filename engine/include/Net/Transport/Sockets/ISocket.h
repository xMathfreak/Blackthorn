#pragma once

#include <cstddef>
#include <memory>

#include "Core/Export.h"
#include "Core/Types/Types.h"
#include "Net/Transport/Address.h"

namespace Blackthorn::Net::Transport::Sockets {

/**
 * @brief Result of a non-blocking socket operation.
 */
enum class SocketResult : U8 {
	Ok, ///< Operation succeeded.
	WouldBlock, ///< No data available (non-blocking recv) or send buffer full.
	Disconnected,  //< Remote peer closed the connection (TCP only).
	Error, ///< Unrecoverable error. Call getLastError() for details.
};

/**
 * @interface ISocket
 * @brief Abstract socket interface.
 *
 * @details All platform-specific socket implementations (@c UDPSocket,
 * @c TCPSocket) implement this interface. Code that deals with the
 * transport layer depends on @c ISocket rather than concrete socket types,
 * making the platform layer fully swappable.
 *
 * @section Lifecycle
 *
 * A newly constructed socket is not yet open. Call @c bind() (server) or
 * @c connect() (client TCP) before sending or receiving. Call @c close()
 * before destruction. The destructor calls @c close() automatically.
 *
 * @section Non-blocking I/O
 *
 * All implementations operate in non-blocking mode after
 * @c setNonBlocking() is called. @c send() and @c recv() return
 * @c SocketResult::WouldBlock rather than blocking when the operation
 * cannot complete immediately.
 */
class BLACKTHORN_API ISocket {
public:
	virtual ~ISocket() = default;

	/**
	 * @brief Binds the socket to a local address.
	 *
	 * For UDP: required before `recv()`.
	 * For TCP servers: required before `listen()` / `accept()`.
	 * For TCP clients: optional (OS assigns an ephemeral port).
	 *
	 * @param address Local address to bind to.
	 * @return true on success.
	 */
	virtual bool bind(const Address& address) = 0;

	/**
	 * @brief Initiates a TCP connection to a remote address.
	 *
	 * Not applicable to UDP sockets - use `sendTo()` directly.
	 *
	 * @param address Remote address to connect to.
	 * @return true if the connection was initiated successfully. In
	 *         non-blocking mode this may return true before the
	 *         handshake completes; poll with `isConnected()`.
	 */
	virtual bool connect(const Address& address) = 0;

	/**
	 * @brief Puts a TCP socket into the listening state.
	 * @param backlog Maximum number of pending connections.
	 * @return true on success.
	 */
	virtual bool listen(int backlog = 8) = 0;

	/**
	 * @brief Accepts an incoming TCP connection.
	 *
	 * @param outAddress Receives the remote peer's address.
	 * @return A new ISocket for the accepted connection, or nullptr if
	 *         no connection is pending (`WouldBlock`) or on error.
	 */
	virtual std::unique_ptr<ISocket> accept(Address& outAddress) = 0;

	/**
	 * @brief Sends data to a specific address (UDP) or the connected peer (TCP).
	 *
	 * @param data    Pointer to the bytes to send.
	 * @param size    Number of bytes.
	 * @param address Destination address (UDP only; ignored for TCP).
	 * @return Result of the operation.
	 */
	virtual SocketResult sendTo(
		const void* data,
		size_t size,
		const Address& address) = 0;

	/**
	 * @brief Sends data to the connected peer (TCP convenience overload).
	 */
	virtual SocketResult send(const void* data, size_t size, size_t& outBytesSent) = 0;

	/**
	 * @brief Receives data from any source (UDP) or the connected peer (TCP).
	 *
	 * @param buffer     Destination buffer.
	 * @param bufferSize Size of the destination buffer in bytes.
	 * @param outSize    Receives the number of bytes actually read.
	 * @param outAddress Receives the sender's address (UDP only).
	 * @return Result of the operation.
	 */
	virtual SocketResult recvFrom(
		void* buffer,
		size_t bufferSize,
		size_t& outSize,
		Address& outAddress) = 0;

	/**
	 * @brief Receives data from the connected peer (TCP convenience overload).
	 */
	virtual SocketResult recv(
		void* buffer,
		size_t bufferSize,
		size_t& outSize) = 0;

	/**
	 * @brief Puts the socket into non-blocking mode.
	 *
	 * Should be called immediately after construction.
	 * @return true on success.
	 */
	virtual bool setNonBlocking(bool enabled = true) = 0;

	/**
	 * @brief Sets the SO_REUSEADDR socket option.
	 *
	 * Allows re-binding to a port that is in TIME_WAIT. Recommended for
	 * server sockets.
	 */
	virtual bool setReuseAddr(bool enabled = true) = 0;

	/**
	 * @brief Closes the socket and releases the file descriptor.
	 *
	 * Safe to call multiple times. Called automatically by the destructor.
	 */
	virtual void close() = 0;

	/** @brief Returns true if the socket is open (not closed). */
	virtual bool isOpen() const = 0;

	/** @brief Returns true if a TCP connection has been established. */
	virtual bool isConnected() const = 0;

	/** @brief Returns the local address the socket is bound to. */
	virtual Address getLocalAddress() const = 0;

	/** @brief Returns a human-readable description of the last error. */
	virtual std::string getLastError() const = 0;
};

} // namespace Blackthorn::Net::Transport::Sockets