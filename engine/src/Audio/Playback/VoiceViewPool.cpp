#include "Audio/Playback/VoiceViewPool.h"

namespace Blackthorn::Audio {

VoiceViewPool::VoiceViewPool(size_t cap) {
	for (auto& buf: buffers)
		buf.resize(cap);
}

VoiceView& VoiceViewPool::writeSlot(size_t slotIndex) {
	return buffers[writeIndex][slotIndex];
}

void VoiceViewPool::publish() {
	const int newSpare =
		spareIndex.exchange(writeIndex, std::memory_order::acq_rel);

	writeIndex = newSpare;
}

void VoiceViewPool::acquire() {
	const int newSpare =
		spareIndex.exchange(readIndex, std::memory_order::acq_rel);

	readIndex = newSpare;
}

const VoiceView& VoiceViewPool::query(
	AudioHandle handle
) const {
	if (!handle.isValid())
		return inactiveDefault;

	for (const VoiceView& view : buffers[readIndex]) {
		if (view.handle.id == handle.id)
			return view;
	}

	return inactiveDefault;
}

size_t VoiceViewPool::capacity() const noexcept {
	return buffers[0].size();
}

} // namespace Blackthorn::Audio