#include "Audio/Backend/AudioBuffer.h"
#include "Audio/AudioException.h"
#include "Debug/Logger.h"

namespace Blackthorn::Audio {

AudioBuffer::~AudioBuffer() {
	destroy();
}

void AudioBuffer::create() {
	if (buffer != 0) {
		BT_WARN("AudioBuffer already exists ({})", buffer);
		return;
	}

	alGenBuffers(1, &buffer);
	BT_DEBUG("AudioBuffer created ({})", buffer);
}

void AudioBuffer::destroy() {
	if (buffer != 0) {
		alDeleteBuffers(1, &buffer);
		buffer = 0;
	}
}

AudioBuffer::AudioBuffer(AudioBuffer&& other) noexcept
	: buffer(other.buffer)
{
	other.buffer = 0;
}

AudioBuffer& AudioBuffer::operator=(AudioBuffer&& other) noexcept {
	if (this == &other)
		return *this;

	if (buffer)
		destroy();

	buffer = other.buffer;
	other.buffer = 0;

	return *this;
}

void AudioBuffer::setData(
	const void* pcm,
	size_t sizeInBytes,
	U16 channels,
	U32 sampleRate
) {
	if (!valid())
		throw AudioException("AudioBuffer: Attempting to call setData on an uninitialized buffer");

	const ALenum format = toOpenALFormat(channels, 16);

	if (!format)
		throw AudioException(
			"AudioBuffer::setData: unsupported channel count " +
			std::to_string(channels)
		);

	alBufferData(
		buffer,
		format,
		pcm,
		static_cast<ALsizei>(sizeInBytes),
		static_cast<ALsizei>(sampleRate)
	);
}

void AudioBuffer::setData(const AudioData& data) {
	setData(
		data.samples.data(),
		data.samples.size() * sizeof(I16),
		static_cast<U16>(data.info.channels),
		data.info.sampleRate
	);
}

ALuint AudioBuffer::get() const noexcept {
	return buffer;
}

bool AudioBuffer::valid() const noexcept {
	return buffer != 0;
}

} // namespace Blackthorn::Audio