#include "Audio/Playback/StreamingVoiceState.h"
#include "Debug/Logger.h"

namespace Blackthorn::Audio {

namespace {

void deleteBuffers(std::array<ALuint, 2>& buffers) {
	for (ALuint& buffer : buffers) {
		if (buffer != 0) {
			alDeleteBuffers(1, &buffer);
			buffer = 0;
		}
	}
}

}

StreamingVoiceState::~StreamingVoiceState() {
	decoder.reset();

	deleteBuffers(alBuffers);

	freeBuffers.clear();
	pendingUpload.clear();

	format = 0;
	sampleRate = 0;

	pendingEndOfStream = false;
	endOfStream = false;

	sourceClip = nullptr;
}

void StreamingVoiceState::init() {
	if (alBuffers[0] != 0 || alBuffers[1] != 0)
		return;

	if (!decoder)
		return;

	auto info = decoder->info();
	format = info.channels == 1
		? AL_FORMAT_MONO16
		: AL_FORMAT_STEREO16;

	sampleRate = info.sampleRate;

	alGenBuffers(
		static_cast<ALsizei>(alBuffers.size()),
		alBuffers.data()
	);

	freeBuffers.clear();
	freeBuffers.reserve(alBuffers.size());

	for (ALuint buffer : alBuffers)
		freeBuffers.push_back(buffer);

	pendingUpload.clear();
	pendingEndOfStream = false;
	endOfStream = false;
}

bool StreamingVoiceState::openDecoder(const AudioClip& clip, const Assets::AssetResolver& resolver) {
	decoder.reset();
	sourceClip = nullptr;

	const std::string id = clip.sourcePath().generic_string();

	decoder = Streaming::StreamDecoderFactory::create(id);
	if (!decoder)
		return false;

	bool opened = false;

	if (clip.hasCompressedSource()) {
		// Small clip: the bytes are already in memory, so stream from them.
		auto stream = std::make_unique<Assets::MemoryAssetStream>(
			clip.compressedData(), clip.compressedSize()
		);

		opened = decoder->openStream(std::move(stream));
	} else {
		// Large clip: stream from the resolver so the file is never fully loaded.
		auto stream = resolver.open(id);
		if (!stream) {
			BT_ERROR("StreamingVoiceState: cannot open stream for '{}'", id);
			decoder.reset();
			return false;
		}

		opened = decoder->openStream(std::move(stream));
	}

	if (!opened) {
		BT_ERROR("StreamingVoiceState: decoder failed to open '{}'", id);
		decoder.reset();
		return false;
	}

	sourceClip = &clip;
	return true;
}

} //namespace Blackthorn::Audio