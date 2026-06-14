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
	const int word = (writeIndex << 1) | 1;
	const int old = spareWord.exchange(word, std::memory_order::release);

	writeIndex = old >> 1;
}

void VoiceViewPool::acquire() {
	const int word = spareWord.load(std::memory_order::relaxed);

	if ((word & 1) == 0)
		return;

	const int old = spareWord.exchange(readIndex << 1, std::memory_order::acquire);
	readIndex = old >> 1;
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