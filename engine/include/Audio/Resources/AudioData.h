#pragma once

#include <vector>

#include <AL/al.h>

#include "Core/Export.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::Audio {

enum class AudioFormat : U8 {
	Mono8,
	Mono16,
	Stereo8,
	Stereo16,
};

inline ALenum toOpenALFormat(
	U16 channels,
	U16 bitsPerSample
) {
	if (channels == 1 && bitsPerSample == 8)
		return AL_FORMAT_MONO8;

	if (channels == 1 && bitsPerSample == 16)
		return AL_FORMAT_MONO16;

	if (channels == 2 && bitsPerSample == 8)
		return AL_FORMAT_STEREO8;

	if (channels == 2 && bitsPerSample == 16)
		return AL_FORMAT_STEREO16;

	return 0;
}

struct BLACKTHORN_API AudioMetadata {
	U64 frameCount = 0;
	U32 sampleRate = 0;
	U32 channels = 0;
};

struct BLACKTHORN_API AudioData {
	std::vector<I16> samples;
	AudioMetadata info;

	[[nodiscard]]
	bool empty() const noexcept {
		return samples.empty();
	}
};

} // namespace Blackthorn::Audio