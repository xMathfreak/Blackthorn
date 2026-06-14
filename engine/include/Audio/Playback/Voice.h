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
		consumedFrames = 0;
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
		spatial = false;
		looped = false;
		userVolume = 1.0f;
		finalGain = 1.0f;
		pitchValue = 1.0f;
		clipDuration = 0.0f;
		playbackTime = 0.0f;
		activationTick = 0;
		decodedFrames = 0;
		consumedFrames = 0;
		audioClip = nullptr;
		sourcePosition = glm::vec3{0.0f};
		minDist = 1.0f;
		maxDist = 50.0f;
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

	/**
	 * @brief Sets the voice volume.
	 *
	 * @param raw  User-supplied scalar in [0, 1]. Stored as @c userVolume
	 *             and used as the base for future gain recomputation when
	 *             category volumes change.
	 * @param gain Final gain applied to the AL source: raw * catVol * masterVol.
	 *             Stored as @c finalGain and published via @c volume() to
	 *             the @c VoiceView.
	 */
	void setVolume(float raw, float gain) {
		userVolume = raw;
		finalGain = gain;
		audioSource.setGain(gain);
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
		sourcePosition = position;
		audioSource.setPosition(position);
	}

	void setDistances(float min, float max) {
		minDist = min;
		maxDist = max;
		audioSource.setDistances(min, max);
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

	void addConsumedFrames(U64 frames) noexcept {
		consumedFrames += frames;
	}

	void setPlaybackTime(float seconds) noexcept {
		playbackTime = seconds;
	}

	void recreateSource() {
		audioSource.destroy();
		audioSource.create();
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

	/**
	 * @brief Returns the final audible gain (raw * category * master).
	 *
	 * This is what the AL source is set to. Published via @c VoiceView::volume
	 * so @c AudioManager::getVolume() returns the actual audible level.
	 */
	[[nodiscard]]
	float volume() const noexcept {
		return finalGain;
	}

	/**
	 * @brief Returns the user-supplied raw volume scalar.
	 *
	 * Used by @c SetCategoryVolumeCommand recomputation and voice snapshots
	 * to avoid compounding multipliers across gain changes.
	 */
	[[nodiscard]]
	float rawVolume() const noexcept {
		return userVolume;
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
	U64 consumedElapsedFrames() const noexcept {
		return consumedFrames;
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

	[[nodiscard]] const glm::vec3& position() const noexcept {
		return sourcePosition;
	}

	[[nodiscard]] float minDistance() const noexcept {
		return minDist;
	}

	[[nodiscard]] float maxDistance() const noexcept {
		return maxDist;
	}

private:
	glm::vec3 sourcePosition{0};

	AudioHandle audioHandle = AudioHandle::invalid();
	AudioSource audioSource;
	U64 activationTick = 0;
	U64 decodedFrames = 0;
	U64 consumedFrames = 0;
	std::unique_ptr<StreamingVoiceState> streamingState = nullptr;
	AudioClip* audioClip = nullptr;

	int voicePriority = 0;
	float pitchValue = 1.0f;
	float clipDuration = 0.0f;
	float playbackTime = 0.0f;
	float userVolume = 1.0f; ///< Raw user scalar. Input to gain computation.
	float finalGain = 1.0f; ///< Computed AL gain: userVolume * catVol * master.
	float minDist = 1.0f;
	float maxDist = 50.0f;

	bool spatial = false;
	bool looped = false;
	AudioCategory audioCategory = AudioCategory::SFX;
};

} // Blackthorn::Audio