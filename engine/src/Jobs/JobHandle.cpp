#include "Jobs/JobHandle.h"

#include <cassert>

namespace Blackthorn::Jobs {

JobHandle::Continuation* const JobHandle::COMPLETE_SENTINEL =
	reinterpret_cast<Continuation*>(static_cast<uintptr_t>(1));

std::shared_ptr<JobHandle> JobHandle::create() {
	return std::shared_ptr<JobHandle>(new JobHandle());
}

void JobHandle::addPending(int count) {
	assert(!isComplete() && "addPending called on a completed handle");
	pendingCount.fetch_add(count, std::memory_order_relaxed);
}

bool JobHandle::isComplete() const {
	return pendingCount.load(std::memory_order_acquire) <= 0;
}

void JobHandle::signal(const std::function<void(std::function<void()>, bool)>& enqueue) {
	if (pendingCount.fetch_sub(1, std::memory_order_acq_rel) != 1)
		return;

	Continuation* list = continuationHead.exchange(COMPLETE_SENTINEL, std::memory_order_acquire);

	while (list && list != COMPLETE_SENTINEL) {
		Continuation* next = list->next;
		enqueue(std::move(list->fn), list->mainThread);
		delete list;
		list = next;
	}
}

void JobHandle::addContinuation(
	std::function<void()> fn,
	bool mainThread,
	const std::function<void(std::function<void()>, bool)>& enqueue)
{
	auto* node = new Continuation{ std::move(fn), mainThread, nullptr };

	Continuation* head = continuationHead.load(std::memory_order_acquire);

	while (true) {
		if (head == COMPLETE_SENTINEL) {
			enqueue(std::move(node->fn), node->mainThread);
			delete node;
			return;
		}

		node->next = head;

		if (continuationHead.compare_exchange_weak(
				head, node,
				std::memory_order_release,
				std::memory_order_acquire))
		{
			return;
		}
	}
}

} // namespace Blackthorn::Jobs