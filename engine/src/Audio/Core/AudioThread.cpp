#include "Audio/Core/AudioThread.h"

#include "Audio/AudioException.h"
#include "Audio/Backend/AudioBuffer.h"
#include "Audio/Device/DeviceNotifierFactory.h"
#include "Audio/Streaming/StreamDecoderFactory.h"
#include "Debug/Logger.h"
#include "Threads/ThreadPriority.h"
#include "Threads/ThreadRegistry.h"

namespace Blackthorn::Audio {

AudioThread::AudioThread()
{
	categoryVolumes.fill(1.0f);
	deviceNotifier = DeviceNotifierFactory::create();
}

AudioThread::~AudioThread() {
	stop();
}

bool AudioThread::start() {
	if (isRunning())
		return true;

	try {
		device.emplace();
		context.emplace(device.value());
	} catch (const AudioException& e) {
		BT_ERROR("AudioThread::start: {}", e.what());
		return false;
	}

	deviceNotifier->setCallback([this](DeviceHint /*hint*/) {
		wakeCv.notify_all();
	});
	deviceNotifier->start();

	state.store(AudioThreadState::Running, std::memory_order::release);
	thread = std::thread([this] { threadLoop(); });

	streamingThread.start(streamResultQueue, wakeCv, wakeMutex);

	return true;
}

void AudioThread::stop() {
	if (state.exchange(AudioThreadState::Stopped, std::memory_order::acq_rel)
		== AudioThreadState::Stopped)
	{
		return;
	}

	streamingThread.stop();

	wakeCv.notify_all();

	if (thread.joinable())
		thread.join();

	deviceNotifier->stop();
}

void AudioThread::enqueue(AudioCommand command) {
	commandQueue.push(std::move(command));
	wakeCv.notify_one();
}

void AudioThread::threadLoop() {
	context->makeCurrent();
	voicePool = std::make_unique<VoicePool>(32);

	#if defined(_WIN32)
		Threads::MmcssScope mmcss;
	#else
		Threads::setAudioThreadPriority();
	#endif

	Threads::ThreadRegistry::instance().registerCurrent("Audio");
	BT_LOG("AudioThread: started");

	while (state.load(std::memory_order::relaxed) != AudioThreadState::Stopped) {
		{
			std::unique_lock lock(wakeMutex);
			wakeCv.wait_for(
				lock,
				std::chrono::milliseconds(5),
				[this] {
					return !commandQueue.empty()
						|| !streamResultQueue.empty()
						|| state.load(std::memory_order::relaxed)
							== AudioThreadState::Stopped;
				}
			);
		}

		drainStreamResults();

		{
			AudioCommand cmd;
			while (commandQueue.pop(cmd))
				processCommand(cmd);
		}

		tickStreaming();
		voicePool->update();
		tick.fetch_add(1, std::memory_order::relaxed);
	}

	drainStreamResults();
	{
		AudioCommand cmd;
		while (commandQueue.pop(cmd))
			processCommand(cmd);
	}

	voicePool->stopAll();
	voicePool.reset();

	BT_LOG("AudioThread: stopped");
	Threads::ThreadRegistry::instance().unregisterCurrent();
}

void AudioThread::drainStreamResults() {
	AudioCommand cmd;
	while (streamResultQueue.pop(cmd))
		processCommand(cmd);
}

void AudioThread::tickStreaming() {
	for (Voice& voice : voicePool->voices()) {
		if (!voice.active() || !voice.streaming())
			continue;

		StreamingVoiceState* sstate = voice.streamState();
		if (!sstate)
			continue;

		voice.source().unqueueProcessedBuffers(sstate->freeBuffers);

		if (!sstate->pendingUpload.empty() && !sstate->freeBuffers.empty()) {
			const ALuint alBuf = sstate->freeBuffers.back();
			sstate->freeBuffers.pop_back();

			alBufferData(
				alBuf,
				sstate->format,
				sstate->pendingUpload.data(),
				static_cast<ALsizei>(
					sstate->pendingUpload.size() * sizeof(I16)
				),
				static_cast<ALsizei>(sstate->sampleRate)
			);
			voice.source().queueBufferId(alBuf);
			sstate->pendingUpload.clear();

			if (sstate->pendingEndOfStream) {
				sstate->endOfStream = true;
				sstate->pendingEndOfStream = false;
				continue;
			}

			submitStreamingJob(voice, *sstate, false);
		}

		if (!sstate->endOfStream && voice.source().isStopped())
			voice.play();
	}
}

void AudioThread::submitStreamingJob(
	Voice& voice,
	StreamingVoiceState& sstate,
	bool previousChunkWasShort
) {
	if (previousChunkWasShort && voice.looping()) {
		if (!sstate.decoder->seek(0)) {
			BT_WARN(
				"AudioThread: loop seek failed for handle {}",
				voice.handle().id
			);
		}
	}

	StreamingJob job;
	job.handle = voice.handle();
	job.decoder = sstate.decoder.get();
	job.frameCount = StreamingThread::DECODE_FRAMES_PER_CHUNK;
	job.channels = (sstate.format == AL_FORMAT_STEREO16) ? 2u : 1u;
	job.sampleRate = sstate.sampleRate;
	job.looping = voice.looping();

	streamingThread.submitJob(std::move(job));
}

void AudioThread::processResidentPlayback(
	Voice& voice,
	const AudioClip& clip
) {
	AudioData data;
	if (!Decoding::AudioDecoder::decode(clip.sourcePath(), data)) {
		BT_ERROR(
			"AudioThread: failed to decode '{}'",
			clip.sourcePath().string()
		);
		voicePool->release(voice);
		return;
	}

	AudioBuffer buffer;
	try {
		buffer.setData(data);
	} catch (const AudioException& e) {
		BT_ERROR(
			"AudioThread: failed to set buffer data: {}",
			e.what()
		);
		voicePool->release(voice);
		return;
	}

	voice.attachBuffer(buffer);
	voice.play();
}

void AudioThread::processStreamingPlayback(
	Voice& voice,
	const AudioClip& clip,
	U64 startTick
) {
	auto decoder = Streaming::StreamDecoderFactory::create(
		clip.sourcePath().string()
	);

	if (!decoder) {
		BT_ERROR(
			"AudioThread: no decoder for '{}'",
			clip.sourcePath().string()
		);
		voicePool->release(voice);
		return;
	}

	if (!decoder->open(clip.sourcePath())) {
		BT_ERROR(
			"AudioThread: failed to open decoder for '{}'",
			clip.sourcePath().string()
		);
		voicePool->release(voice);
		return;
	}

	auto sstate = std::make_unique<StreamingVoiceState>();
	sstate->format = (clip.channels() == 2)
		? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;

	sstate->sampleRate = clip.sampleRate();
	sstate->sourceClip = &clip;
	sstate->decoder = std::move(decoder);
	sstate->init();

	if (startTick > 0) {
		if (!sstate->decoder->seek(startTick)) {
			BT_WARN(
				"AudioThread: seek to frame {} failed for '{}', "
				"resuming from start",
				startTick, clip.sourcePath().string()
			);
		}
	}

	const bool looping = voice.looping();
	int queued = 0;
	U64 prefillFrames = 0;

	while (!sstate->freeBuffers.empty()) {
		const ALuint alBuf = sstate->freeBuffers.back();
		sstate->freeBuffers.pop_back();

		const size_t frames = prefillBuffer(*sstate, alBuf, looping);

		if (frames == 0) {
			sstate->freeBuffers.push_back(alBuf);
			break;
		}

		voice.source().queueBufferId(alBuf);
		prefillFrames += frames;
		++queued;
	}

	if (queued == 0) {
		BT_ERROR(
			"AudioThread: pre-fill produced no frames for '{}', aborting",
			clip.sourcePath().string()
		);
		voicePool->release(voice);
		return;
	}

	voice.attachStreamingState(std::move(sstate));
	voice.addDecodedFrames(prefillFrames);
	voice.play();

	submitStreamingJob(voice, *voice.streamState(), false);
}

size_t AudioThread::prefillBuffer(
	StreamingVoiceState& sstate,
	ALuint alBuffer,
	bool looping
) {
	const size_t maxFrames = StreamingThread::DECODE_FRAMES_PER_CHUNK;
	const U32 channels = (sstate.format == AL_FORMAT_STEREO16) ? 2u : 1u;

	std::vector<I16> pcm(maxFrames * channels);

	size_t framesRead = sstate.decoder->readFrames(pcm.data(), maxFrames);

	if (framesRead == 0 && looping) {
		sstate.decoder->seek(0);
		framesRead = sstate.decoder->readFrames(pcm.data(), maxFrames);
	}

	if (framesRead == 0)
		return 0;

	alBufferData(
		alBuffer,
		sstate.format,
		pcm.data(),
		static_cast<ALsizei>(framesRead * channels * sizeof(I16)),
		static_cast<ALsizei>(sstate.sampleRate)
	);

	return framesRead;
}

void AudioThread::processCommand(const AudioCommand& command) {
	std::visit(
		[this](auto&& cmd) { process(cmd); },
		command
	);
}

} // namespace Blackthorn::Audio