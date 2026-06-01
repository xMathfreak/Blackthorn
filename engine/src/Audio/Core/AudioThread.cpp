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
	: voicePool(0)
{
	deviceNotifier = DeviceNotifierFactory::create();
}

AudioThread::~AudioThread() {
	stop();
}

bool AudioThread::start() {
	device.emplace();
	context.emplace(device.value());

	context->makeCurrent();
	voicePool = VoicePool();

	deviceNotifier->start();
	deviceNotifier->setCallback([this](DeviceHint hint) {
		wakeCv.notify_all();
	});

	thread = std::thread([this] { threadLoop(); });
	state.store(AudioThreadState::Running);
	return true;
}

void AudioThread::stop() {
	if (state.exchange(AudioThreadState::Stopped) != AudioThreadState::Stopped)
		return;

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
	Threads::setAudioThreadPriority();

	Threads::ThreadRegistry::instance().registerCurrent("Audio");
	BT_LOG("AudioThread: Audio thread started");

	while (state.load() != AudioThreadState::Stopped) {
		{
			std::unique_lock lock(wakeMutex);
			wakeCv.wait_for(
				lock,
				std::chrono::milliseconds(5),
				[this] { return state.load() != AudioThreadState::Stopped; }
			);
		}

		{
			AudioCommand cmd;
			while (commandQueue.pop(cmd))
				processCommand(cmd);
		}

		tickStreaming();
		voicePool.update();
		tick.fetch_add(1);
	}

	if (state.load() == AudioThreadState::Running) {
		AudioCommand cmd;
		while (commandQueue.pop(cmd))
			processCommand(cmd);
	}

	BT_LOG("AudioThread: Audio thread stopped");
	Threads::ThreadRegistry::instance().unregisterCurrent();
}

void AudioThread::tickStreaming() {
	for (Voice& voice : voicePool.voices()) {
		if (!voice.active() || !voice.streaming())
			continue;

		auto sstate = voice.streamState();

		if (!sstate)
			continue;

		voice.source().unqueueProcessedBuffers(sstate->freeBuffers);

		if (!sstate->pendingUpload.empty() && !sstate->freeBuffers.empty()) {
			const ALuint alBuf = sstate->freeBuffers.back();
			sstate->freeBuffers.pop_back();

			const U32 channels =
				(sstate -> format == AL_FORMAT_STEREO16) ? 2 : 1;

			alBufferData(
				alBuf,
				sstate->format,
				sstate->pendingUpload.data(),
				static_cast<ALsizei>(sstate->pendingUpload.size() * sizeof(I16)),
				static_cast<ALsizei>(sstate->sampleRate)
			);

			voice.source().queueBufferId(alBuf);
			sstate->pendingUpload.clear();

			if (sstate->pendingEndOfStream) {
				sstate->endOfStream = true;
				sstate->pendingEndOfStream = false;
				continue;
			}

			// Create and upload streaming job
			(void)channels;
		}

		if (voice.source().isStopped() && !sstate->endOfStream)
			voice.play();
	}
}

void AudioThread::processCommand(const AudioCommand& command) {
	std::visit(
		[this](auto&& cmd) { process(cmd); },
		command
	);
}

void AudioThread::processResidentPlayback(
	Voice& voice,
	const AudioClip& clip
) {
	AudioData data;
	if (!Decoding::AudioDecoder::decode(clip.sourcePath(), data)) {
		BT_ERROR("AudioThread: failed to decode {}", clip.sourcePath().string());
		voicePool.release(voice);
		return;
	}

	AudioBuffer buffer;
	try {
		buffer.setData(data);
	} catch (const AudioException& e) {
		BT_ERROR("AudioThread: Failed to set audio buffer data: {}", e.what());
		voicePool.release(voice);
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
			"AudioThread: No decoder for '{}'",
			clip.sourcePath().string()
		);
		voicePool.release(voice);
		return;
	}

	if (!decoder->open(clip.sourcePath())) {
		BT_ERROR(
			"AudioThread: Failed to open decoder for '{}'",
			clip.sourcePath().string()
		);
		voicePool.release(voice);
		return;
	}

	auto sstate = std::make_unique<StreamingVoiceState>();

	sstate->format = (clip.channels() == 2)
		? AL_FORMAT_STEREO16
		: AL_FORMAT_MONO16;
	sstate->sampleRate = clip.sampleRate();
	sstate->sourceClip = &clip;
	sstate->decoder = std::move(decoder);

	sstate->init();

	if (startTick > 0) {
		if (!sstate->decoder->seek(startTick)) {
			BT_WARN(
				"AudioThread: failed to seek decoder to frame {} in '{}', "
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

		const size_t framesDecoded = prefillBuffer(*sstate, alBuf, looping);

		if (framesDecoded == 0) {
			sstate->freeBuffers.push_back(alBuf);
			break;
		}

		voice.source().queueBufferId(alBuf);
		prefillFrames += framesDecoded;
		++queued;
	}

	if (queued == 0) {
		BT_ERROR(
			"AudioThread: Pre-fill produced no frames for '{}', aborting",
			clip.sourcePath().string()
		);
		voicePool.release(voice);
		return;
	}

	voice.attachStreamingState(std::move(sstate));
	voice.addDecodedFrames(prefillFrames);
	voice.play();

	StreamingVoiceState* liveState = voice.streamState();
	// Create and upload streaming job
	(void)liveState;
}

size_t AudioThread::prefillBuffer(
	StreamingVoiceState& sstate,
	ALuint alBuffer,
	bool looping
) {
	// const size_t maxFrames = StreamingThread::DECODE_FRAMES_PER_CHUNK;
	const size_t maxFrames = 4096;
	const U32 channels = (sstate.format == AL_FORMAT_STEREO16) ? 2 : 1;

	std::vector<I16> pcm(maxFrames * channels);

	size_t framesRead = sstate.decoder->readFrames(
		pcm.data(),
		maxFrames
	);

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

} // namespace Blackthorn::Audio