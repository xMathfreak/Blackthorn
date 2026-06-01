#include "Audio/Core/AudioThread.h"

#include <AL/al.h>

#include "Audio/AudioCategory.h"
#include "Audio/Resources/AudioClip.h"
#include "Audio/Playback/StreamingVoiceState.h"
#include "Debug/Logger.h"

namespace Blackthorn::Audio {

namespace {

float computeGain(
	float baseVolume,
	AudioCategory category,
	const std::array<float, AUDIO_CATEGORY_COUNT>& categoryVolumes
) {
	const float catVol = categoryVolumes[
		static_cast<size_t>(category)
	];
	const float masterVol = categoryVolumes[
		static_cast<size_t>(AudioCategory::Master)
	];
	return baseVolume * catVol * masterVol;
}

void uploadAndQueue(
	AudioSource& source,
	StreamingVoiceState& state,
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
	source.queueBufferId(alBuf);
}

} // namespace

void AudioThread::process(const PlayCommand& cmd) {
	Voice* voice = voicePool.acquire(cmd.priority);

	if (!voice) {
		BT_WARN("AudioThread: voice pool exhausted, dropping PlayCommand");
		return;
	}

	voice->activate(
		cmd.handle,
		cmd.category,
		cmd.priority,
		tick.load(std::memory_order::relaxed),
		cmd.clipSource->durationSeconds()
	);

	voice->setVolume(
		computeGain(cmd.volume, cmd.category, categoryVolumes)
	);
	voice->setPitch(cmd.pitch);
	voice->setLooping(cmd.loop);

	if (cmd.spatial) {
		voice->setPosition(cmd.position);
		voice->source().setRelative(false);
	} else {
		voice->source().setRelative(true);
	}

	voice->source().setDistances(cmd.minDistance, cmd.maxDistance);

	// Force as resident for now
	if (!cmd.clipSource) {
		BT_WARN("PlayCommand has null or unloaded clip");
		voicePool.release(*voice);
		return;
	}

	processResidentPlayback(*voice, *cmd.clipSource);
	voice->setClip(cmd.clipSource);

	// std::visit(
	// 	[this, &voice](auto&& src) {
	// 		using T = std::decay_t<decltype(src)>;

	// 		if constexpr (std::is_same_v<T, ResidentClipSource>) {
	// 			if (!src.clip || !src.clip->isLoaded()) {
	// 				BT_WARN(
	// 					"AudioThread: PlayCommand has null or "
	// 					"unloaded resident clip"
	// 				);
	// 				voicePool.release(*voice);
	// 				return;
	// 			}
	// 			processResidentPlayback(*voice, *src.clip);
	// 			voice->setClipRef(src.clip);

	// 		} else if constexpr (std::is_same_v<T, StreamingClipSource>) {
	// 			if (!src.clip || !src.clip->isLoaded()) {
	// 				BT_WARN(
	// 					"AudioThread: PlayCommand has null or "
	// 					"unloaded streaming clip"
	// 				);
	// 				voicePool.release(*voice);
	// 				return;
	// 			}
	// 			processStreamingPlayback(*voice, *src.clip);
	// 			if (voice->streaming())
	// 				voice->setClipRef(src.clip);
	// 		}
	// 	},
	// 	cmd.clipSource
	// );
}

void AudioThread::process(const StopCommand& cmd) {
	Voice* voice = voicePool.find(cmd.handle);

	if (voice)
		voicePool.release(*voice);
}

void AudioThread::process(const PauseCommand& cmd) {
	Voice* voice = voicePool.find(cmd.handle);

	if (voice)
		voice->pause();
}

void AudioThread::process(const ResumeCommand& cmd) {
	Voice* voice = voicePool.find(cmd.handle);

	if (voice)
		voice->resume();
}

void AudioThread::process(const SetVolumeCommand& cmd) {
	Voice* voice = voicePool.find(cmd.handle);

	if (!voice)
		return;

	voice->setVolume(
		computeGain(cmd.volume, voice->category(), categoryVolumes)
	);
}

void AudioThread::process(const SetPitchCommand& cmd) {
	Voice* voice = voicePool.find(cmd.handle);

	if (voice)
		voice->setPitch(cmd.pitch);
}

void AudioThread::process(const SetPositionCommand& cmd) {
	Voice* voice = voicePool.find(cmd.handle);

	if (voice)
		voice->setPosition(cmd.position);
}

void AudioThread::process(const SetCategoryVolumeCommand& cmd) {
	const size_t idx = static_cast<size_t>(cmd.category);

	if (idx >= AUDIO_CATEGORY_COUNT) {
		BT_WARN("AudioThread: SetCategoryVolumeCommand has invalid category");
		return;
	}

	categoryVolumes[idx] = cmd.volume;

	const float masterVol = categoryVolumes[
		static_cast<size_t>(AudioCategory::Master)
	];

	for (Voice& voice : voicePool.voices()) {
		if (!voice.active())
			continue;

		if (voice.category() != cmd.category &&
			cmd.category != AudioCategory::Master)
			continue;

		const float catVol = categoryVolumes[
			static_cast<size_t>(voice.category())
		];

		voice.source().setGain(voice.volume() * catVol * masterVol);
	}
}

void AudioThread::process(const ListenerTransformCommand& cmd) {
	alListener3f(AL_POSITION,
		cmd.position.x, cmd.position.y, cmd.position.z);
	alListener3f(AL_VELOCITY,
		cmd.velocity.x, cmd.velocity.y, cmd.velocity.z);

	const ALfloat orientation[6] = {
		cmd.forward.x, cmd.forward.y, cmd.forward.z,
		cmd.up.x, cmd.up.y, cmd.up.z
	};
	alListenerfv(AL_ORIENTATION, orientation);
}

void AudioThread::process(const StopAllCommand&) {
	voicePool.stopAll();
}

void AudioThread::process(const StreamBufferReadyCommand& cmd) {
	Voice* voice = voicePool.find(cmd.handle);

	if (!voice || !voice->streaming())
		return;

	StreamingVoiceState* sstate = voice->streamState();

	const U32 channels =
		(sstate->format == AL_FORMAT_STEREO16) ? 2 : 1;

	voice->source().unqueueProcessedBuffers(sstate->freeBuffers);

	if (!cmd.samples.empty()) {
		const U64 framesInChunk =
			static_cast<U64>(cmd.samples.size()) / channels;
		voice->addDecodedFrames(framesInChunk);
	}

	if (sstate->freeBuffers.empty()) {
		sstate->pendingUpload = cmd.samples;
		sstate->pendingEndOfStream = cmd.endOfStream;
		return;
	}

	if (!cmd.samples.empty()) {
		const ALuint alBuf = sstate->freeBuffers.back();
		sstate->freeBuffers.pop_back();
		uploadAndQueue(voice->source(), *sstate, alBuf, cmd.samples);
	}

	if (cmd.endOfStream) {
		sstate->endOfStream = true;
		return;
	}

	// const size_t fullChunkSamples =
	// 	StreamingThread::DECODE_FRAMES_PER_CHUNK *
	// 	static_cast<size_t>(channels);

	// if (voice->looping() && cmd.samples.size() < fullChunkSamples)
	// 	sstate->decoder->seek(0);

	// StreamingJob job;
	// job.handle = voice->handle();
	// job.decoder = sstate->decoder.get();
	// job.frameCount = StreamingThread::DECODE_FRAMES_PER_CHUNK;
	// job.channels = channels;
	// job.sampleRate = sstate->sampleRate;
	// job.looping = voice->looping();

	// streamingThread.submitJob(std::move(job));
}

} // namespace Blackthorn::Audio