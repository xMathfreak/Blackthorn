#pragma once

#include <vector>

#include <AL/al.h>
#include <glm/glm.hpp>

#include "Core/Export.h"

namespace Blackthorn::Audio {

class AudioBuffer;

class BLACKTHORN_API AudioSource {
public:
	AudioSource();
	~AudioSource();

	AudioSource(const AudioSource&) = delete;
	AudioSource& operator=(const AudioSource&) = delete;

	AudioSource(AudioSource&& other) noexcept;
	AudioSource& operator=(AudioSource&& other) noexcept;

	bool create();
	void destroy();

	void play();
	void pause();
	void stop();

	void setLooping(bool looping);

	void setStreamingMode(bool streaming) noexcept;

	void setGain(float gain);
	void setPitch(float pitch);
	void setPosition(const glm::vec3& pos);
	void setRelative(bool relative);
	void setDistances(float minDistance, float maxDistance);
	void useDirectChannel(bool enabled);

	void attachBuffer(const AudioBuffer& buffer);
	void detachBuffer();
	void queueBuffer(const AudioBuffer& buffer);
	void queueBufferId(ALuint bufferId);
	void unqueueProcessedBuffers(std::vector<ALuint>& out);
	void unqueueAllBuffers();

	[[nodiscard]] ALuint get() const noexcept;
	[[nodiscard]] bool isPlaying() const;
	[[nodiscard]] bool isStopped() const;
	[[nodiscard]] int processedBuffers() const;
	[[nodiscard]] int queuedBuffers() const;

	[[nodiscard]] bool valid() const noexcept;

	/**
	 * Marks the source handle as no longer valid without issuing AL calls.
	 *
	 * Used during device/context loss where the underlying OpenAL source
	 * has already been implicitly destroyed by the backend.
	 */
	void invalidate() noexcept;

private:
	ALuint source = 0;
	bool streamingMode = false;
};

} // namespace Blackthorn::Audio