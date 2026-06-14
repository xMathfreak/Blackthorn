#include "Audio/Playback/VoicePool.h"

#include "Audio/Playback/StreamingVoiceState.h"

namespace Blackthorn::Audio {

VoicePool::VoicePool(size_t maxVoices)
	: voiceStorage(maxVoices)
{}

Voice* VoicePool::findFreeSlot() {
	for (Voice& voice : voiceStorage) {
		if (!voice.active())
			return &voice;
	}

	return nullptr;
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
	if (Voice* free = findFreeSlot())
		return free;

	Voice* stolen = findStealCandidate(priority);

	if (!stolen)
		return nullptr;

	release(*stolen);
	return stolen;
}

Voice* VoicePool::find(AudioHandle handle) {
	if (!handle.isValid())
		return nullptr;

	for (Voice& voice : voiceStorage) {
		if (voice.active() && voice.handle().id == handle.id)
			return &voice;
	}

	return nullptr;
}

void VoicePool::release(Voice& voice) {
	voice.reset();
}

void VoicePool::stopAll() {
	for (Voice& voice : voiceStorage) {
		if (voice.active())
			release(voice);
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

void VoicePool::initSources() {
	for (Voice& voice : voiceStorage)
		voice.source().create();
}

} // namespace Blackthorn::Audio