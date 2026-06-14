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

bool AudioThread::start(const AudioConfig& cfg) {
	if (isRunning())
		return true;

	config = cfg;

	try {
		device.emplace();
		context.emplace(device.value());
	} catch (const AudioException& e) {
		BT_ERROR("AudioThread::start: {}", e.what());
		return false;
	}

	deviceNotifier->setCallback([this](DeviceHint hint) {
		if (hint == DeviceHint::DefaultDeviceChanged ||
			hint == DeviceHint::DeviceArrived)
		{
			pendingMigrationHint.store(true, std::memory_order::release);
		}

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

VoiceViewPool& AudioThread::views() const noexcept {
	return *viewPool;
}

void AudioThread::threadLoop() {
	context->makeCurrent();

	voicePool = std::make_unique<VoicePool>(config.maxVoices);
	voicePool->initSources();

	viewPool = std::make_unique<VoiceViewPool>(config.maxVoices);

	#if defined(_WIN32)
		Threads::MmcssScope mmcss;
	#else
		Threads::setAudioThreadPriority();
	#endif

	Threads::ThreadRegistry::instance().registerCurrent("Audio");
	BT_LOG("AudioThread: started");

	while (state.load(std::memory_order::relaxed) != AudioThreadState::Stopped) {
		const AudioThreadState currentState = state.load(std::memory_order::relaxed);

		const bool inRecovery =
			currentState == AudioThreadState::DeviceLost ||
			currentState == AudioThreadState::Migrating;

		const auto sleepDuration = inRecovery
			? std::chrono::milliseconds(50)
			: std::chrono::milliseconds(5);

		{
			std::unique_lock lock(wakeMutex);
			wakeCv.wait_for(
				lock,
				sleepDuration,
				[this] {
					return !commandQueue.empty()
						|| !streamResultQueue.empty()
						|| state.load(std::memory_order::relaxed)
							== AudioThreadState::Stopped;
				}
			);
		}

		tickDeviceHealth();

		if (state.load(std::memory_order::relaxed) == AudioThreadState::Running) {
			drainStreamResults();

			{
				AudioCommand cmd;
				while (commandQueue.pop(cmd))
					processCommand(std::move(cmd));
			}

			tickStreaming();
			tickViews();
			voicePool->update();
			updatePlaybackTimes();
		}

		tick.fetch_add(1, std::memory_order::relaxed);
	}

	if (state.load(std::memory_order::relaxed) == AudioThreadState::Running) {
		drainStreamResults();
		AudioCommand cmd;
		while (commandQueue.pop(cmd))
			processCommand(std::move(cmd));
	}

	voicePool->stopAll();
	voicePool.reset();
	viewPool.reset();

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

		const size_t prevFreeCount = sstate->freeBuffers.size();
		voice.source().unqueueProcessedBuffers(sstate->freeBuffers);

		for (size_t i = prevFreeCount; i < sstate->freeBuffers.size(); ++i) {
			const U64 frames =
				sstate->lookupBufferFrames(sstate->freeBuffers[i]);
			voice.addConsumedFrames(frames);
		}

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

			const U64 pendingFrames =
				static_cast<U64>(sstate->pendingUpload.size()) /
				((sstate->format == AL_FORMAT_STEREO16) ? 2u : 1u);

			sstate->recordBufferFrames(alBuf, pendingFrames);
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

void AudioThread::tickViews() {
	const size_t cap = viewPool->capacity();
	const auto& voices = voicePool->voices();

	for (size_t i = 0; i < cap && i < voices.size(); ++i) {
		const Voice& voice = voices[i];
		VoiceView& view = viewPool->writeSlot(i);

		if (!voice.active()) {
			view = VoiceView{};
			continue;
		}

		view.handle = voice.handle();

		if (voice.source().isPlaying()) {
			view.state = PlaybackState::Playing;
		} else if (voice.source().isStopped()) {
			view.state = PlaybackState::Stopped;
		} else {
			view.state = PlaybackState::Paused;
		}

		view.flags.reset();

		if (voice.looping())
			view.flags.set(VoiceFlagBit::Looping);

		if (voice.spatialized())
			view.flags.set(VoiceFlagBit::Spatial);

		if (voice.streaming())
			view.flags.set(VoiceFlagBit::Streaming);

		view.duration = voice.duration();
		view.playbackPosition = voice.getPlaybackTime();
		view.volume = voice.volume();
		view.pitch = voice.pitch();
	}

	viewPool->publish();
}

void AudioThread::updatePlaybackTimes() {
	for (Voice& voice : voicePool->voices()) {
		if (!voice.active())
			continue;

		if (voice.streaming()) {
			const StreamingVoiceState* sstate = voice.streamState();
			if (!sstate || sstate->sampleRate == 0)
				continue;

			ALint sampleOffset = 0;
			alGetSourcei(
				voice.source().get(),
				AL_SAMPLE_OFFSET,
				&sampleOffset
			);

			const U64 totalFrames =
				voice.consumedElapsedFrames() +
				static_cast<U64>(sampleOffset);

			const float t =
				static_cast<float>(totalFrames) /
				static_cast<float>(sstate->sampleRate);

			const float duration = voice.duration();
			const float position =
				(voice.looping() && duration > 0.0f)
					? std::fmod(t, duration)
					: t;

			voice.setPlaybackTime(position);
		} else {
			ALfloat seconds = 0.0f;
			alGetSourcef(
				voice.source().get(),
				AL_SEC_OFFSET,
				&seconds
			);
			voice.setPlaybackTime(seconds);
		}
	}
}

void AudioThread::tickDeviceHealth() {
	const AudioThreadState currentState =
		state.load(std::memory_order::relaxed);

	if (currentState == AudioThreadState::Running) {
		if (!device || !device->connected()) {
			BT_WARN("AudioThread: device lost, entering recovery");
			enterRecovery(AudioThreadState::DeviceLost);
			return;
		}

		if (pendingMigrationHint.exchange(false, std::memory_order::acq_rel)) {
			if (defaultDeviceChanged()) {
				BT_LOG("AudioThread: default device changed, migrating");
				enterRecovery(AudioThreadState::Migrating);
			}
		}

		return;
	}

	if (currentState == AudioThreadState::DeviceLost ||
		currentState == AudioThreadState::Migrating
	) {
		const auto now = std::chrono::steady_clock::now();
		if (now < nextRetryTime)
			return;

		state.store(AudioThreadState::Reconnecting,
			std::memory_order::relaxed);
		attemptReconnect(currentState);
		return;
	}
}

void AudioThread::enterRecovery(AudioThreadState reason) {
	outageStartTime = std::chrono::steady_clock::now();
	backoffIndex = 0;

	const int firstDelay = (reason == AudioThreadState::Migrating)
		? kMigrationBackoffMs[0]
		: kLossBackoffMs[0];

	nextRetryTime = outageStartTime +
		std::chrono::milliseconds(firstDelay);

	voiceSnapshots.clear();

	for (Voice& voice : voicePool->voices()) {
		if (!voice.active())
			continue;

		if (shouldRestoreVoice(voice)) {
			VoiceSnapshot snap;

			snap.originalHandle = voice.handle();
			snap.clip = voice.clip();
			snap.volume = voice.rawVolume();
			snap.pitch = voice.pitch();
			snap.playbackTime = voice.getPlaybackTime();
			snap.duration = voice.duration();
			snap.position = voice.position();
			snap.minDistance = voice.minDistance();
			snap.maxDistance = voice.maxDistance();
			snap.category = voice.category();
			snap.priority = voice.priority();
			snap.loop = voice.looping();
			snap.spatial = voice.spatialized();
			snap.stream = voice.streaming();

			voiceSnapshots.push_back(std::move(snap));
		}

		voice.source().invalidate();
	}

	BT_LOG(
		"AudioThread: snapshotted {} voice(s) for restore",
		voiceSnapshots.size()
	);

	streamingThread.stop();
	voicePool->stopAll();

	context.reset();
	device.reset();

	state.store(reason, std::memory_order::relaxed);
}

void AudioThread::attemptReconnect(AudioThreadState returnStateOnFailure) {
	const bool isMigration =
		(returnStateOnFailure == AudioThreadState::Migrating);

	BT_LOG(
		"AudioThread: attempting {} (backoff index {})",
		isMigration ? "migration" : "reconnect",
		backoffIndex
	);

	try {
		device.emplace();
		context.emplace(device.value());
		context->makeCurrent();
	} catch (const AudioException& e) {
		BT_WARN(
			"AudioThread: {} attempt failed: {}",
			isMigration ? "migration" : "reconnect",
			e.what()
		);

		context.reset();
		device.reset();

		if (isMigration) {
			backoffIndex = std::min(
				backoffIndex + 1,
				kMigrationBackoffMs.size() - 1
			);
			nextRetryTime = std::chrono::steady_clock::now() +
				std::chrono::milliseconds(
					kMigrationBackoffMs[backoffIndex]
				);
		} else {
			backoffIndex = std::min(
				backoffIndex + 1,
				kLossBackoffMs.size() - 1
			);
			nextRetryTime = std::chrono::steady_clock::now() +
				std::chrono::milliseconds(
					kLossBackoffMs[backoffIndex]
				);
		}

		state.store(returnStateOnFailure, std::memory_order::relaxed);
		return;
	}

	backoffIndex = 0;

	for (Voice& voice : voicePool->voices())
		voice.recreateSource();

	streamingThread.start(streamResultQueue, wakeCv, wakeMutex);

	restoreVoices();
	voiceSnapshots.clear();

	state.store(AudioThreadState::Running, std::memory_order::relaxed);
	BT_DEBUG("AudioThread: Reconnect successful");
}

void AudioThread::restoreVoices() {
	const auto now = std::chrono::steady_clock::now();
	const float outageDuration =
		std::chrono::duration<float>(now - outageStartTime).count();

	for (const VoiceSnapshot& snap : voiceSnapshots) {
		if (!snap.clip || !snap.clip->isLoaded())
			continue;

		float effectiveTime = snap.playbackTime + outageDuration;

		if (snap.loop && snap.duration > 0.0f) {
			effectiveTime = std::fmod(effectiveTime, snap.duration);
		} else {
			effectiveTime = std::min(effectiveTime, snap.duration);
		}

		if (!snap.loop && snap.duration > 0.0f) {
			const float remaining = snap.duration - effectiveTime;
			if (remaining < kMinRemainingTime)
				continue;
		}

		Voice* voice = voicePool->acquire(snap.priority);
		if (!voice) {
			BT_WARN(
				"AudioThread: restoreVoices: pool exhausted, "
				"dropping snapshot for handle {}",
				snap.originalHandle.id
			);
			continue;
		}

		voice->activate(
			snap.originalHandle,
			snap.category,
			snap.priority,
			tick.load(std::memory_order::relaxed),
			snap.duration
		);

		voice->setVolume(
			snap.volume,
			computeGain(snap.volume, snap.category, categoryVolumes)
		);

		voice->setPitch(snap.pitch);
		voice->source().setStreamingMode(snap.stream);
		voice->setLooping(snap.loop);

		if (snap.spatial) {
			voice->setPosition(snap.position);
			voice->source().setRelative(false);
		} else {
			voice->source().setRelative(true);
		}

		voice->setDistances(snap.minDistance, snap.maxDistance);
		voice->setClip(const_cast<AudioClip*>(snap.clip));

		if (snap.stream) {
			const U64 startFrame = static_cast<U64>(
				effectiveTime *
				static_cast<float>(snap.clip->sampleRate())
			);
			processStreamingPlayback(*voice, *snap.clip, startFrame);
		} else {
			processResidentPlayback(*voice, *snap.clip, effectiveTime);
		}

		BT_DEBUG(
			"AudioThread: restored voice {} at {:.2f}s "
			"(was {:.2f}s, outage {:.2f}s)",
			snap.originalHandle.id,
			effectiveTime,
			snap.playbackTime,
			outageDuration
		);
	}
}

bool AudioThread::defaultDeviceChanged() const noexcept {
	if (!device || !device->valid())
		return false;

	const ALCchar* defaultName =
		alcGetString(nullptr, ALC_ALL_DEVICES_SPECIFIER);

	if (!defaultName)
		return false;

	return device->getDeviceName() != defaultName;
}

bool AudioThread::shouldRestoreVoice(const Voice& voice) const noexcept {
	if (voice.category() == AudioCategory::SFX ||
		voice.category() == AudioCategory::UI)
	{
		return false;
	}

	if (voice.looping())
		return true;

	const float remaining = voice.duration() - voice.getPlaybackTime();
	return remaining > kMinRemainingTime;
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

	voice.markJobInFlight();

	streamingThread.submitJob(std::move(job));
}

void AudioThread::processResidentPlayback(
	Voice& voice,
	const AudioClip& clip,
	float seekSeconds
) {
	AudioBuffer buffer;
	buffer.create();

	try {
		if (clip.hasPCM()) {
			buffer.setData(clip.data(), clip.metadata());
		} else {
			BT_WARN(
				"AudioThread: decoding '{}' on audio thread, call loadPCM() before play() to avoid this",
				clip.sourcePath().string()
			);

			AudioData data;
			if (!Decoding::AudioDecoder::decode(clip.sourcePath(), data)) {
				BT_ERROR(
					"AudioThread: Failed to decode '{}'",
					clip.sourcePath().string()
				);
				voicePool->release(voice);
				return;
			}

			buffer.setData(data, clip.metadata());
		}
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

	if (seekSeconds > 0.0f) {
		alSourcef(
			voice.source().get(),
			AL_SEC_OFFSET,
			seekSeconds
		);
	}
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
		voice.addConsumedFrames(startTick);
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

void AudioThread::processCommand(AudioCommand command) {
	std::visit(
		[this](auto&& c) { process(std::forward<decltype(c)>(c)); },
		std::move(command)
	);
}

} // namespace Blackthorn::Audio