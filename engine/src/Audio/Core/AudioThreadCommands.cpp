#include "Audio/Core/AudioThread.h"

#include <AL/al.h>

#include "Audio/AudioCategory.h"
#include "Audio/Playback/StreamingVoiceState.h"
#include "Audio/Resources/AudioClip.h"
#include "Debug/Logger.h"

namespace Blackthorn::Audio {

namespace {

void uploadAndQueue(
	Voice& voice,
	StreamingVoiceState&  state,
	ALuint alBuf,
	const std::vector<I16>& samples
) {
	alBufferData(
		alBuf,
		state.format,
		samples.data(),
		static_cast<ALsizei>(samples.size() * sizeof(I16)),
		static_cast<ALsizei>(state.sampleRate)
	);

	const U64 frames =
		static_cast<U64>(samples.size()) /
		((state.format == AL_FORMAT_STEREO16) ? 2u : 1u);
	state.recordBufferFrames(alBuf, frames);

	voice.source().queueBufferId(alBuf);
}

} // namespace

void AudioThread::process(PlayCommand&& cmd) {
	if (!cmd.clipSource) {
		BT_WARN("AudioThread: PlayCommand has null clip, dropping");
		return;
	}

	Voice* voice = voicePool->acquire(cmd.priority);

	if (!voice) {
		BT_WARN("AudioThread: voice pool exhausted, dropping PlayCommand");
		return;
	}

	voicePool->activate(
		*voice,
		cmd.handle,
		cmd.category,
		cmd.priority,
		tick.load(std::memory_order::relaxed),
		static_cast<float>(cmd.clipSource->durationSeconds())
	);

	voice->setVolume(
		cmd.volume,
		computeGain(cmd.volume, cmd.category, categoryVolumes)
	);
	voice->setPitch(cmd.pitch);
	voice->source().setStreamingMode(cmd.stream);
	voice->setLooping(cmd.loop);

	if (cmd.spatial) {
		voice->setPosition(cmd.position);
		voice->source().setRelative(false);
	} else {
		voice->source().setRelative(true);
	}

	voice->setDistances(cmd.minDistance, cmd.maxDistance);
	voice->setClip(cmd.clipSource);

	if (cmd.stream)
		processStreamingPlayback(*voice, *cmd.clipSource, 0);
	else
		processResidentPlayback(*voice, *cmd.clipSource);
}

void AudioThread::process(StopCommand cmd) {
	Voice* voice = voicePool->find(cmd.handle);

	if (!voice)
		return;

	if (voice->streaming() && voice->hasJobInFlight()) {
		voice->markPendingStop();
		return;
	}

	voicePool->release(*voice);
}

void AudioThread::process(PauseCommand cmd) {
	Voice* voice = voicePool->find(cmd.handle);

	if (voice)
		voice->pause();
}

void AudioThread::process(ResumeCommand cmd) {
	Voice* voice = voicePool->find(cmd.handle);

	if (voice)
		voice->resume();
}

void AudioThread::process(SetVolumeCommand cmd) {
	Voice* voice = voicePool->find(cmd.handle);

	if (!voice)
		return;

	voice->setVolume(
		cmd.volume,
		computeGain(cmd.volume, voice->category(), categoryVolumes)
	);
}

void AudioThread::process(SetPitchCommand cmd) {
	Voice* voice = voicePool->find(cmd.handle);

	if (voice)
		voice->setPitch(cmd.pitch);
}

void AudioThread::process(SetPositionCommand cmd) {
	Voice* voice = voicePool->find(cmd.handle);

	if (voice)
		voice->setPosition(cmd.position);
}

void AudioThread::process(SetCategoryVolumeCommand cmd) {
	const size_t idx = static_cast<size_t>(cmd.category);

	if (idx >= AUDIO_CATEGORY_COUNT) {
		BT_WARN("AudioThread: SetCategoryVolumeCommand has invalid category");
		return;
	}

	categoryVolumes[idx] = cmd.volume;

	for (Voice& voice : voicePool->voices()) {
		if (!voice.active())
			continue;

		if (voice.category() != cmd.category &&
			cmd.category != AudioCategory::Master)
			continue;

		voice.setVolume(
			voice.rawVolume(),
			computeGain(voice.rawVolume(), voice.category(), categoryVolumes)
		);
	}
}

void AudioThread::process(ListenerTransformCommand cmd) {
	alListener3f(AL_POSITION,
		cmd.position.x, cmd.position.y, cmd.position.z);
	alListener3f(AL_VELOCITY,
		cmd.velocity.x, cmd.velocity.y, cmd.velocity.z);

	const ALfloat orientation[6] = {
		cmd.forward.x, cmd.forward.y, cmd.forward.z,
		cmd.up.x,      cmd.up.y,      cmd.up.z
	};
	alListenerfv(AL_ORIENTATION, orientation);
}

void AudioThread::process(StopAllCommand) {
	voicePool->stopAll();
}

void AudioThread::process(StreamBufferReadyCommand&& cmd) {
	Voice* voice = voicePool->find(cmd.handle);

	if (!voice || !voice->streaming())
		return;

	voice->clearJobInFlight();

	if (voice->isPendingStop()) {
		voicePool->release(*voice);
		return;
	}

	if (voice->isPendingSeek()) {
		float target = voice->pendingSeekSeconds();
		voice->clearPendingSeek();
		performStreamingSeek(*voice, target);
		return;
	}

	StreamingVoiceState* sstate = voice->streamState();

	const U32 channels =
		(sstate->format == AL_FORMAT_STEREO16) ? 2u : 1u;

	voice->source().unqueueProcessedBuffers(sstate->freeBuffers);

	if (!cmd.samples.empty()) {
		const U64 framesInChunk =
			static_cast<U64>(cmd.samples.size()) / channels;
		voice->addDecodedFrames(framesInChunk);
	}

	if (sstate->freeBuffers.empty() && !cmd.samples.empty()) {
		sstate->pendingUpload = std::move(cmd.samples);
		sstate->pendingEndOfStream = cmd.endOfStream;
		return;
	}

	if (!cmd.samples.empty()) {
		const ALuint alBuf = sstate->freeBuffers.back();
		sstate->freeBuffers.pop_back();
		uploadAndQueue(*voice, *sstate, alBuf, cmd.samples);
	}

	if (cmd.endOfStream) {
		sstate->endOfStream = true;
		return;
	}

	const size_t fullChunkSamples =
		StreamingThread::DECODE_FRAMES_PER_CHUNK *
		static_cast<size_t>(channels);

	const bool shortRead =
		cmd.samples.empty() ||
		cmd.samples.size() < fullChunkSamples;

	submitStreamingJob(*voice, *sstate, shortRead);
}

void AudioThread::process(SeekCommand cmd) {
	Voice* voice = voicePool->find(cmd.handle);

	if (!voice)
		return;

	if (voice->streaming()) {
		if (voice->hasJobInFlight()) {
			voice->markPendingSeek(cmd.seconds);
			return;
		}

		performStreamingSeek(*voice, cmd.seconds);
		return;
	}

	const bool wasPlaying = voice->source().isPlaying();

	if (wasPlaying)
		voice->source().stop();

	alSourcef(voice->source().get(), AL_SEC_OFFSET, cmd.seconds);
	voice->setPlaybackTime(cmd.seconds);

	if (wasPlaying)
		voice->source().play();
}

void AudioThread::performStreamingSeek(Voice& voice, float seconds) {
	StreamingVoiceState* sstate = voice.streamState();
	if (!sstate || !sstate->decoder)
		return;

	const bool wasPlaying = voice.source().isPlaying();

	voice.source().stop();
	voice.source().unqueueAllBuffers();

	sstate->freeBuffers.clear();
	for (ALuint buf : sstate->alBuffers)
		sstate->freeBuffers.push_back(buf);

	sstate->pendingUpload.clear();
	sstate->pendingEndOfStream = false;
	sstate->endOfStream = false;

	const U64 targetFrame = static_cast<U64>(
		seconds * static_cast<float>(sstate->sampleRate)
	);

	voice.resetFrameCounters(targetFrame);
	voice.setPlaybackTime(seconds);

	if (!sstate->decoder->seek(targetFrame))
		BT_WARN("AudioThread: streaming seek to {:.2f}s failed for handle {}",
			seconds, voice.handle().id);

	int queued = 0;
	U64 prefillFrames = 0;

	while (!sstate->freeBuffers.empty()) {
		const ALuint buf = sstate->freeBuffers.back();
		sstate->freeBuffers.pop_back();

		const size_t frames = prefillBuffer(*sstate, buf, voice.looping());
		if (frames == 0) {
			sstate->freeBuffers.push_back(buf);
			break;
		}

		voice.source().queueBufferId(buf);
		prefillFrames += frames;
		++queued;
	}

	if (queued == 0) {
		sstate->endOfStream = true;
		return;
	}

	voice.addDecodedFrames(prefillFrames);

	if (wasPlaying)
		voice.source().play();

	submitStreamingJob(voice, *sstate, false);
}

} // namespace Blackthorn::Audio