#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

namespace Blackthorn::Containers {

/**
 * @brief Lock-free single-producer single-consumer ring queue.
 *
 * Requirements:
 * - Exactly ONE producer thread may call push/emplace.
 * - Exactly ONE consumer thread may call pop.
 * - Capacity must be a power of two.
 *
 * Notes:
 * - Queue stores at most Capacity - 1 elements.
 * - Elements are constructed in-place only when occupied.
 * - No heap allocations are performed by the queue itself.
 */
template <typename T, size_t Capacity>
class SPSCRingQueue {
	static_assert(Capacity >= 2,
		"SPSCRingQueue: Capacity must be >= 2");

	static_assert(
		(Capacity & (Capacity - 1)) == 0,
		"SPSCRingQueue: Capacity must be a power of two"
	);

public:
	SPSCRingQueue() = default;

	~SPSCRingQueue() {
		atomic_thread_fence(std::memory_order::seq_cst);
		clear();
	}

	SPSCRingQueue(const SPSCRingQueue&) = delete;
	SPSCRingQueue& operator=(const SPSCRingQueue&) = delete;

	/**
	 * @brief Push an element into the queue.
	 *
	 * @return false if the queue is full.
	 */
	bool push(T&& value) {
		const size_t h = head.load(std::memory_order::relaxed);
		const size_t next = increment(h);

		if (next == tail.load(std::memory_order::acquire))
			return false;

		new (&storage[h]) T(std::move(value));

		head.store(next, std::memory_order::release);

		return true;
	}

	/**
	 * @brief Construct an element directly in the queue.
	 *
	 * @return false if the queue is full.
	 */
	template <typename... Args>
	bool emplace(Args&&... args) {
		const size_t h = head.load(std::memory_order::relaxed);
		const size_t next = increment(h);

		if (next == tail.load(std::memory_order::acquire))
			return false;

		new (&storage[h]) T(std::forward<Args>(args)...);

		head.store(next, std::memory_order::release);

		return true;
	}

	/**
	 * @brief Pop the next element from the queue.
	 *
	 * @return false if the queue is empty.
	 */
	bool pop(T& out) {
		const size_t t = tail.load(std::memory_order::relaxed);

		if (t == head.load(std::memory_order::acquire))
			return false;

		T* ptr = ptr_at(t);

		out = std::move(*ptr);

		ptr->~T();

		tail.store(increment(t), std::memory_order::release);

		return true;
	}

	/**
	 * @brief Destroy all remaining elements.
	 */
	void clear() noexcept {
		size_t t = tail.load(std::memory_order::relaxed);
		const size_t h = head.load(std::memory_order::relaxed);

		while (t != h) {
			ptr_at(t)->~T();
			t = increment(t);
		}

		tail.store(t, std::memory_order::relaxed);
	}

	[[nodiscard]]
	bool empty() const noexcept {
		return head.load(std::memory_order::acquire)
			== tail.load(std::memory_order::acquire);
	}

	[[nodiscard]]
	bool full() const noexcept {
		const size_t h = head.load(std::memory_order::acquire);
		const size_t next = increment(h);

		return next == tail.load(std::memory_order::acquire);
	}

	[[nodiscard]]
	size_t size() const noexcept {
		const size_t h = head.load(std::memory_order::acquire);
		const size_t t = tail.load(std::memory_order::acquire);

		return (h - t) & MASK;
	}

	[[nodiscard]]
	static constexpr size_t capacity() noexcept {
		return Capacity - 1;
	}

private:
	static constexpr size_t MASK = Capacity - 1;

	using Storage =
		std::aligned_storage_t<
			sizeof(T),
			alignof(T)
		>;

	static constexpr size_t increment(size_t index) noexcept {
		return (index + 1) & MASK;
	}

	T* ptr_at(size_t index) noexcept {
		return std::launder(
			reinterpret_cast<T*>(&storage[index])
		);
	}

	const T* ptr_at(size_t index) const noexcept {
		return std::launder(
			reinterpret_cast<const T*>(&storage[index])
		);
	}

	alignas(64) std::atomic<size_t> head{ 0 };
	alignas(64) std::atomic<size_t> tail{ 0 };

	alignas(64) std::array<Storage, Capacity> storage;
};

} // namespace Blackthorn::Containers