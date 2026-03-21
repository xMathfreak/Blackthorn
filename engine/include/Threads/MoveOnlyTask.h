#pragma once

#include <memory>
#include <type_traits>

namespace Blackthorn::Threads {

struct ITask {
	virtual ~ITask() = default;
	virtual void invoke() = 0;
};

template <typename Callable>
struct Task final : ITask {
	static_assert(
		std::is_invocable_v<Callable>,
		"Task<Callable>: Callable must be invocable with no arguments"
	);

	explicit Task(Callable&& c) : callable(std::move(c)) {}

	void invoke() override { callable(); }

	Callable callable;
};

using TaskPtr = std::unique_ptr<ITask>;

/**
 * @brief Constructs a Task<Callable> and returns it as a TaskPtr.
 *
 * Callable is deduced by the forwarding reference; std::decay_t strips
 * reference and cv-qualifiers so we always store a value type.
 */
template <typename Callable>
requires std::invocable<Callable>
[[nodiscard]] TaskPtr makeTask(Callable&& callable) {
	return std::make_unique<Task<std::decay_t<Callable>>>(
		std::forward<Callable>(callable)
	);
}

} // namespace Blackthorn::Threads