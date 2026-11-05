#pragma once

#include <AL/alc.h>

#include "Core/Export.h"

namespace Blackthorn::Audio {

class AudioDevice;

class BLACKTHORN_API AudioContext {
public:
	explicit AudioContext(ALCdevice* device);
	AudioContext(const AudioDevice& device);

	~AudioContext();

	AudioContext(const AudioContext&) = delete;
	AudioContext& operator=(const AudioContext&) = delete;

	AudioContext(AudioContext&& other) noexcept;
	AudioContext& operator=(AudioContext&& other) noexcept;

	void makeCurrent() const;

	[[nodiscard]]
	ALCcontext* get() const noexcept;

	bool create(ALCdevice* device);
	void destroy();

private:
	ALCcontext* context = nullptr;
};

} // namespace Blackthorn::Audio