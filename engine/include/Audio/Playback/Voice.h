#pragma once

#include "Audio/AudioCategory.h"
#include "Audio/AudioHandle.h"
#include "Audio/Backend/AudioSource.h"
#include "Audio/Playback/StreamingVoiceState.h"
#include "Audio/Resources/AudioClip.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::Audio {

class Voice {
public:
	void activate(
		AudioHandle handle,
		AudioCategory category,
		int priority,
		U64 tick,
		float duration
	) {
		audioHandle = handle;
		audioCategory = category;
		voicePriority = priority;
		activationTick = tick;
		clipDuration = duration;
		decodedFrames = 0;
		playbackTime = 0.0f;
		audioClip = nullptr;
	}

	void reset() {
		audioSource.stop();
		audioSource.unqueueAllBuffers();
		audioSource.detachBuffer();
		audioSource.setStreamingMode(false);

		streamingState.reset();

		audioHandle = AudioHandle::invalid();
		audioCategory = AudioCategory::SFX;
		voicePriority = 0;
		trackVoice = false;
		spatial = false;
		looped = false;
		baseVolume = 1.0f;
		pitchValue = 1.0f;
		clipDuration = 0.0f;
		playbackTime = 0.0f;
		activationTick = 0;
		decodedFrames = 0;
		audioClip = nullptr;
	}

	void play() {
		audioSource.play();
	}

	void stop() {
		audioSource.stop();
	}

	void pause() {
		audioSource.pause();
	}

	void resume() {
		audioSource.play();
	}

	void setVolume(float volume) {
		baseVolume = volume;
		audioSource.setGain(volume);
	}

	void setPitch(float pitch) {
		pitchValue = pitch;
		audioSource.setPitch(pitch);
	}

	void setLooping(bool looping) {
		looped = looping;
		audioSource.setLooping(looping);
	}

	void setPosition(const glm::vec3& position) {
		spatial = true;
		audioSource.setPosition(position);
	}

	void attachBuffer(const AudioBuffer& buffer) {
		audioSource.attachBuffer(buffer);
		audioSource.setStreamingMode(false);
	}

	void attachStreamingState(std::unique_ptr<StreamingVoiceState> state) {
		streamingState = std::move(state);
		audioSource.setStreamingMode(true);
	}

	void detachStreamingState() {
		streamingState.reset();
		audioSource.setStreamingMode(false);
	}

	void setClip(AudioClip* clip) noexcept {
		audioClip = clip;
	}

	const AudioClip* clip() const noexcept {
		return audioClip;
	}

	void addDecodedFrames(U64 frames) noexcept {
		decodedFrames += frames;
	}

	void setPlaybackTime(float seconds) noexcept {
		playbackTime = seconds;
	}

	[[nodiscard]]
	bool active() const noexcept {
		return audioHandle.isValid();
	}

	[[nodiscard]]
	bool streaming() const noexcept {
		return streamingState != nullptr;
	}

	[[nodiscard]]
	bool spatialized() const noexcept {
		return spatial;
	}

	[[nodiscard]]
	bool tracked() const noexcept {
		return trackVoice;
	}

	[[nodiscard]]
	bool looping() const noexcept {
		return looped;
	}

	[[nodiscard]]
	int priority() const noexcept {
		return voicePriority;
	}

	[[nodiscard]]
	U64 startTick() const noexcept {
		return activationTick;
	}

	[[nodiscard]]
	float volume() const noexcept {
		return baseVolume;
	}

	[[nodiscard]]
	float pitch() const noexcept {
		return pitchValue;
	}

	[[nodiscard]]
	float duration() const noexcept {
		return clipDuration;
	}

	[[nodiscard]]
	U64 elapsedFrames() const noexcept {
		return decodedFrames;
	}

	[[nodiscard]]
	float getPlaybackTime() const noexcept {
		return playbackTime;
	}

	[[nodiscard]] AudioHandle handle() const noexcept {
		return audioHandle;
	}

	[[nodiscard]] AudioCategory category() const noexcept {
		return audioCategory;
	}

	[[nodiscard]] AudioSource& source() noexcept {
		return audioSource;
	}

	[[nodiscard]] const AudioSource& source() const noexcept {
		return audioSource;
	}

	[[nodiscard]] StreamingVoiceState* streamState() noexcept {
		return streamingState.get();
	}

	[[nodiscard]] const StreamingVoiceState* streamState() const noexcept {
		return streamingState.get();
	}

private:
	AudioHandle audioHandle = AudioHandle::invalid();
	AudioSource audioSource;
	U64 activationTick = 0;
	U64 decodedFrames = 0;
	std::unique_ptr<StreamingVoiceState> streamingState = nullptr;
	AudioClip* audioClip = nullptr;

	int voicePriority = 0;
	float pitchValue = 1.0f;
	float clipDuration = 0.0f;
	float playbackTime = 0.0f;
	float baseVolume = 1.0f;

	bool spatial = false;
	bool looped = false;
	bool trackVoice = false;
	AudioCategory audioCategory = AudioCategory::SFX;
};

} // Blackthorn::Audio