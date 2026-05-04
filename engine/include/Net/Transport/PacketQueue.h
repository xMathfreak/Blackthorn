#pragma once

#include <array>
#include <atomic>
#include <cstddef>

#include "Core/Export.h"
#include "Core/Types/Numeric.h"
#include "IO/ByteBuffer.h"
#include "Net/Connection/NetworkPeer.h"
#include "Net/Transport/Address.h"

namespace Blackthorn::Net::Transport {

/**
 * @brief A received packet staged for consumption by the simulation thread.
 */
struct BLACKTHORN_API InboundPacket {
	/// Source address of the packet.
	Address source;

	/// Full datagram bytes (UDPHeader + PacketHeader + payload for UDP,
	/// or PacketHeader + payload for TCP).
	IO::ByteBuffer data;

	/// Identifies which channel this packet arrived on.
	enum class Channel : U8 { UDP, TCP } channel = Channel::UDP;

	/// The peer that sent this packet, or INVALID_PEER_ID if unknown.
	Connection::PeerId peerId = Connection::INVALID_PEER_ID;
};

/**
 * @brief Lock-free single-producer single-consumer (SPSC) inbound packet queue.
 *
 * @details The I/O thread is the sole producer (@c push), and the simulation
 * thread is the sole consumer (@c pop). No mutex is required because only one
 * thread writes the head index and only one thread reads the tail index.
 *
 * @par Capacity
 *
 * The queue holds up to @c Capacity - 1 packets. @c Capacity must be a power
 * of two so that index wrapping can be performed with a single bitwise AND.
 *
 * When the queue is full, @c push() returns false and the packet is dropped.
 * Increase @c Capacity or drain the queue faster if this becomes an issue.
 *
 * @par Memory layout
 *
 * Slots are pre-allocated in a @c std::array, so no heap allocation occurs
 * during normal operation. Each @c InboundPacket contains a @c ByteBuffer
 * whose internal @c std::vector may allocate on first use, but subsequent
 * packets of the same size reuse the existing allocation after @c clear().
 *
 * @tparam Capacity Maximum number of packets in flight at once.
 *                  Must be a power of two. Default: 256.
 */
template <size_t Capacity = 256>
class PacketQueue {
	static_assert((Capacity & (Capacity - 1)) == 0,
		"PacketQueue Capacity must be a power of two");

public:
	PacketQueue() {
		head.store(0, std::memory_order::relaxed);
		tail.store(0, std::memory_order::relaxed);
	}

	PacketQueue(const PacketQueue&) = delete;
	PacketQueue& operator=(const PacketQueue&) = delete;

	/**
	 * @brief Pushes a packet into the queue from the I/O thread.
	 *
	 * @param packet The packet to enqueue. Moved into the slot.
	 * @return true if the packet was enqueued, false if the queue is full.
	 *
	 * @note Must only be called from the I/O thread (single producer).
	 */
	bool push(InboundPacket packet) {
		const size_t h = head.load(std::memory_order::relaxed);
		const size_t next = (h + 1) & MASK;

		if (next == tail.load(std::memory_order::acquire))
			return false;

		slots[h] = std::move(packet);
		head.store(next, std::memory_order::release);
		return true;
	}

	/**
	 * @brief Pops the next packet from the queue into `out`.
	 *
	 * @param out Receives the front-of-queue packet on success.
	 * @return true if a packet was dequeued, false if the queue is empty.
	 *
	 * @note Must only be called from the simulation thread (single consumer).
	 */
	bool pop(InboundPacket& out) {
		const size_t t = tail.load(std::memory_order::relaxed);

		if (t == head.load(std::memory_order::acquire))
			return false;

		out = std::move(slots[t]);
		tail.store((t + 1) & MASK, std::memory_order::release);
		return true;
	}

	/** @brief Returns true if no packets are queued. */
	bool empty() const noexcept {
		return head.load(std::memory_order::acquire)
			== tail.load(std::memory_order::acquire);
	}

	/**
	 * @brief Returns the approximate number of packets currently queued.
	 *
	 * Approximate because the producer may push between the two loads.
	 */
	size_t size() const noexcept {
		const size_t h = head.load(std::memory_order::acquire);
		const size_t t = tail.load(std::memory_order::acquire);

		return (h - t) & MASK;
	}

	static constexpr size_t capacity() { return Capacity - 1; }

private:
	static constexpr size_t MASK = Capacity - 1;

	// Pad head and tail to separate cache lines to avoid false sharing
	// between the producer (I/O thread) and consumer (sim thread).
	alignas(64) std::atomic<size_t> head{0};
	alignas(64) std::atomic<size_t> tail{0};

	std::array<InboundPacket, Capacity> slots;
};

/// Default instantiation used by ConnectionManager.
using DefaultPacketQueue = PacketQueue<256>;

} // namespace Blackthorn::Net::Transport