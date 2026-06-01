#include "Audio/Backend/AudioSource.h"

#include <AL/al.h>
#include <alext.h>

#include "Audio/AudioException.h"
#include "Audio/Backend/AudioBuffer.h"

namespace Blackthorn::Audio {

AudioSource::AudioSource() {
	create();
}

AudioSource::~AudioSource() {
	destroy();
}

bool AudioSource::create() {
	alGenSources(1, &source);
	return (source != 0);
}

void AudioSource::destroy() {
	if (source != 0) {
		stop();
		unqueueAllBuffers();
		alDeleteSources(1, &source);
		source = 0;
	}
}

bool AudioSource::valid() const noexcept {
	return (source != 0);
}

void AudioSource::invalidate() noexcept {
	source = 0;
}

AudioSource::AudioSource(AudioSource&& other) noexcept
	: source(other.source)
	, streamingMode(other.streamingMode)
{
	other.source = 0;
	other.streamingMode = false;
}

AudioSource& AudioSource::operator=(AudioSource&& other) noexcept {
	if (this == &other)
		return *this;

	if (source)
		alDeleteSources(1, &source);

	source = other.source;
	streamingMode = other.streamingMode;
	other.source = 0;
	other.streamingMode = false;

	return *this;
}

void AudioSource::play() { alSourcePlay(source); }
void AudioSource::pause() { alSourcePause(source); }
void AudioSource::stop() { alSourceStop(source); }

void AudioSource::setLooping(bool looping) {
	if (!streamingMode)
		alSourcei(source, AL_LOOPING, looping ? AL_TRUE : AL_FALSE);
}

void AudioSource::setStreamingMode(bool streaming) noexcept {
	streamingMode = streaming;
}

void AudioSource::setGain(float gain) {
	alSourcef(source, AL_GAIN, gain);
}

void AudioSource::setPitch(float pitch) {
	alSourcef(source, AL_PITCH, pitch);
}

void AudioSource::setPosition(const glm::vec3& pos) {
	alSource3f(source, AL_POSITION, pos.x, pos.y, pos.z);
}

void AudioSource::setRelative(bool relative) {
	alSourcei(source, AL_SOURCE_RELATIVE, relative ? AL_TRUE : AL_FALSE);
}

void AudioSource::setDistances(
	float minDistance,
	float maxDistance
) {
	alSourcef(source, AL_REFERENCE_DISTANCE, minDistance);
	alSourcef(source, AL_MAX_DISTANCE, maxDistance);
}

void AudioSource::attachBuffer(const AudioBuffer& buffer) {
	if (streamingMode) {
		throw AudioException("Cannot call attachBuffer() on streaming source");
	}

	alSourcei(
		source,
		AL_BUFFER,
		static_cast<ALint>(buffer.get())
	);
}

void AudioSource::detachBuffer() {
	alSourcei(source, AL_BUFFER, 0);
}

void AudioSource::queueBuffer(const AudioBuffer& buffer) {
	const ALuint id = buffer.get();
	alSourceQueueBuffers(source, 1, &id);
}

void AudioSource::queueBufferId(ALuint bufferId) {
	alSourceQueueBuffers(source, 1, &bufferId);
}

void AudioSource::unqueueProcessedBuffers(std::vector<ALuint>& out) {
	ALint processed = 0;
	alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);

	if (processed <= 0)
		return;

	const size_t offset = out.size();
	out.resize(offset + static_cast<size_t>(processed));
	alSourceUnqueueBuffers(source, processed, out.data() + offset);
}

void AudioSource::unqueueAllBuffers() {
	ALint queued = 0;
	alGetSourcei(source, AL_BUFFERS_QUEUED, &queued);

	if (queued <= 0)
		return;

	std::vector<ALuint> tmp(static_cast<size_t>(queued));
	alSourceUnqueueBuffers(source, queued, tmp.data());
}

ALuint AudioSource::get() const noexcept {
	return source;
}

bool AudioSource::isPlaying() const {
	ALint state = 0;
	alGetSourcei(source, AL_SOURCE_STATE, &state);
	return state == AL_PLAYING;
}

bool AudioSource::isStopped() const {
	ALint state = 0;
	alGetSourcei(source, AL_SOURCE_STATE, &state);
	return state == AL_STOPPED;
}

int AudioSource::processedBuffers() const {
	ALint n = 0;
	alGetSourcei(source, AL_BUFFERS_PROCESSED, &n);
	return n;
}

int AudioSource::queuedBuffers() const {
	ALint n = 0;
	alGetSourcei(source, AL_BUFFERS_QUEUED, &n);
	return n;
}

void AudioSource::useDirectChannel(bool enabled) {
	if (alIsExtensionPresent("AL_SOFT_direct_channels")) {
		alSourcei(source, AL_DIRECT_CHANNELS_SOFT, enabled ? AL_TRUE : AL_FALSE);
	}
}

} // namespace Blackthorn::Audio