#include "Audio/Backend/AudioContext.h"

#include <AL/alext.h>

#include "Audio/AudioException.h"
#include "Audio/Backend/AudioDevice.h"

namespace Blackthorn::Audio {

AudioContext::AudioContext(ALCdevice* device) {
	if (!create(device))
		throw AudioException("Failed to create OpenAL context");
}

AudioContext::AudioContext(const AudioDevice& device) {
	if (!create(device.get()))
		throw AudioException("Failed to create OpenAL context");
}

AudioContext::~AudioContext() {
	destroy();
}

bool AudioContext::create(ALCdevice* device) {
	ALCint attribs[] = {
		ALC_HRTF_SOFT, ALC_FALSE,
		ALC_OUTPUT_LIMITER_SOFT, ALC_FALSE,
		0
	};

	context = alcCreateContext(device, attribs);
	return (context != nullptr);
}

void AudioContext::destroy() {
	if (context) {
		alcDestroyContext(context);
		context = nullptr;
	}
}

AudioContext::AudioContext(AudioContext&& other) noexcept
	: context(other.context) {
	other.context = nullptr;
}

AudioContext& AudioContext::operator=(AudioContext&& other) noexcept {
	if (this == &other)
		return *this;

	if (context)
		alcDestroyContext(context);

	context = other.context;
	other.context = nullptr;

	return *this;
}

void AudioContext::makeCurrent() const {
	if (!alcMakeContextCurrent(context)) {
		throw AudioException("Failed to make OpenAL context current");
	}
}

ALCcontext* AudioContext::get() const noexcept {
	return context;
}

} // namespace Blackthorn::Audio