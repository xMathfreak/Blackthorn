#include "Audio/Playback/VoicePool.h"

#include "Audio/Playback/StreamingVoiceState.h"

namespace Blackthorn::Audio {

VoicePool::VoicePool(size_t maxVoices)
	: voiceStorage(maxVoices)
	, handleIds(maxVoices, 0)
{
	freeSlots.reserve(maxVoices);

	for (size_t i = maxVoices; i-- > 0;)
		freeSlots.push_back(i);
}

Voice* VoicePool::findStealCandidate(int incomingPriority) {
	Voice* candidate = nullptr;

	for (Voice& voice : voiceStorage) {
		if (!voice.active())
			continue;

		if (voice.priority() >= incomingPriority)
			continue;

		if (!candidate) {
			candidate = &voice;
			continue;
		}

		if (voice.priority() < candidate->priority()
			|| (voice.priority() == candidate->priority()
			&& voice.startTick() < candidate->startTick())
		) {
			candidate = &voice;
		}
	}

	return candidate;
}

Voice* VoicePool::acquire(int priority) {
	if (!freeSlots.empty()) {
		const size_t idx = freeSlots.back();
		freeSlots.pop_back();
		return &voiceStorage[idx];
	}

	Voice* stolen = findStealCandidate(priority);

	if (!stolen)
		return nullptr;

	release(*stolen);

	const size_t idx = freeSlots.back();
	freeSlots.pop_back();
	return &voiceStorage[idx];
}

Voice* VoicePool::find(AudioHandle handle) {
	if (!handle.isValid())
		return nullptr;

	for (size_t i = 0; i < handleIds.size(); ++i) {
		if (handleIds[i] == handle.id)
			return &voiceStorage[i];
	}

	return nullptr;
}

void VoicePool::release(Voice& voice) {
	const size_t idx = static_cast<size_t>(&voice - voiceStorage.data());

	voice.reset();
	handleIds[idx] = 0;
	freeSlots.push_back(idx);
}

void VoicePool::stopAll() {
	freeSlots.clear();

	for (size_t i = 0; i < voiceStorage.size(); ++i) {
		if (voiceStorage[i].active())
			voiceStorage[i].reset();

		handleIds[i] = 0;
		freeSlots.push_back(i);
	}
}

void VoicePool::update() {
	for (Voice& voice : voiceStorage) {
		if (!voice.active())
			continue;

		if (voice.streaming()) {
			const StreamingVoiceState* state =
				voice.streamState();

			if (state && state->endOfStream && voice.source().isStopped())
				release(voice);

			continue;
		}

		if (voice.source().isStopped())
			release(voice);
	}
}

size_t VoicePool::size() const noexcept {
	return voiceStorage.size();
}

const std::vector<Voice>& VoicePool::voices() const noexcept {
	return voiceStorage;
}

std::vector<Voice>& VoicePool::voices() noexcept {
	return voiceStorage;
}

} // namespace Blackthorn::Audio