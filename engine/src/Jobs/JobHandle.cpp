#include "Jobs/JobHandle.h"

#include <cassert>

namespace Blackthorn::Jobs {

JobHandle::Continuation* const JobHandle::COMPLETE_SENTINEL =
	reinterpret_cast<Continuation*>(static_cast<uintptr_t>(1));

std::shared_ptr<JobHandle> JobHandle::create() {
	return std::shared_ptr<JobHandle>(new JobHandle());
}

std::shared_ptr<JobHandle> JobHandle::createComplete() {
	auto h = create();
	h->signal([](std::function<void()>, bool) {});
	return h;
}

void JobHandle::addPending(int count) {
	assert(!isComplete() && "addPending called on a completed handle");
	pendingCount.fetch_add(count, std::memory_order::relaxed);
}

bool JobHandle::isComplete() const {
	return pendingCount.load(std::memory_order::acquire) <= 0;
}

void JobHandle::signal(const std::function<void(std::function<void()>, bool)>& enqueue) {
	if (pendingCount.fetch_sub(1, std::memory_order::acq_rel) != 1)
		return;

	Continuation* list = nullptr;

	{
		std::lock_guard lock(continuationMutex);
		continuationHead.exchange(COMPLETE_SENTINEL, std::memory_order::acq_rel);
	}

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
	bool runImmediately = false;

	{
		std::lock_guard lock(continuationMutex);

		Continuation* head = continuationHead.load(std::memory_order::relaxed);

		if (head == COMPLETE_SENTINEL) {
			runImmediately = true;
		} else {
			auto* node = new Continuation{ std::move(fn), mainThread, head };
			continuationHead.store(node, std::memory_order::relaxed);
		}
	}

	if (runImmediately)
		enqueue(std::move(fn), mainThread);
}

} // namespace Blackthorn::Jobs