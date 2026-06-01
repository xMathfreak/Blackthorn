#include "Audio/Playback/StreamingVoiceState.h"

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

} //namespace Blackthorn::Audio