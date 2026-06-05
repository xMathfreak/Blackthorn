#include "Audio/AudioException.h"
#include "Debug/Logger.h"

#include <AL/al.h>
#include <string>

namespace Blackthorn::Audio {

const char* alErrorString(ALenum err) noexcept {
	switch (err) {
		case AL_NO_ERROR:
			return "AL_NO_ERROR";
		case AL_INVALID_NAME:
			return "AL_INVALID_NAME";
		case AL_INVALID_ENUM:
			return "AL_INVALID_ENUM";
		case AL_INVALID_VALUE:
			return "AL_INVALID_VALUE";
		case AL_INVALID_OPERATION:
			return "AL_INVALID_OPERATION";
		case AL_OUT_OF_MEMORY:
			return "AL_OUT_OF_MEMORY";
		default:
			return "AL_UNKNOWN_ERROR";
	}
}

void checkOpenALError(const char* context) {
	const ALenum err = alGetError();

	if (err == AL_NO_ERROR)
		return;

	const char* name = alErrorString(err);

	if (context && context[0] != '\0') {
		BT_ERROR("{}: OpenAL error {} (0x{:X})", context, name, err);
		throw AudioException(
			std::string(context) +
			": OpenAL error " + name +
			" (0x" + std::to_string(err) + ")"
		);
	}

	BT_ERROR("OpenAL error {} (0x{:X})", name, err);
	throw AudioException(
		std::string("OpenAL error ") + name +
		" (0x" + std::to_string(err) + ")"
	);
}


} // namespace Blackthorn::Audio