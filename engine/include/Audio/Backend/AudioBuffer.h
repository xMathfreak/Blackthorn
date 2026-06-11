#pragma once

#include <AL/al.h>

#include "Audio/Resources/AudioData.h"

namespace Blackthorn::Audio {

class BLACKTHORN_API AudioBuffer {
public:
	/**
	 * @brief Constructs an empty, invalid buffer.
	 *
	 * Does not call any OpenAL functions. Call @c create() explicitly
	 * once an OpenAL context is current on the calling thread.
	 */
	AudioBuffer() = default;
	~AudioBuffer();

	AudioBuffer(const AudioBuffer&) = delete;
	AudioBuffer& operator=(const AudioBuffer&) = delete;

	AudioBuffer(AudioBuffer&& other) noexcept;
	AudioBuffer& operator=(AudioBuffer&& other) noexcept;

	void create();
	void destroy();

	void setData(
		const void* pcm,
		size_t sizeInBytes,
		U16 channels,
		U32 sampleRate
	);

	void setData(const AudioData& data);

	[[nodiscard]]
	ALuint get() const noexcept;

	[[nodiscard]]
	bool valid() const noexcept;

private:
	ALuint buffer = 0;
};

} // namespace Blackthorn::Audio
