#pragma once

#include <string>

#include <AL/alc.h>

#include "Core/Export.h"

namespace Blackthorn::Audio {

class BLACKTHORN_API AudioDevice {
public:
	AudioDevice();
	explicit AudioDevice(const char* name);
	~AudioDevice();

	AudioDevice(const AudioDevice&) = delete;
	AudioDevice& operator=(const AudioDevice&) = delete;

	AudioDevice(AudioDevice&& other) noexcept;
	AudioDevice& operator=(AudioDevice&& other) noexcept;

	[[nodiscard]]
	ALCdevice* get() const noexcept;

	[[nodiscard]]
	bool valid() const noexcept;

	[[nodiscard]]
	std::string getDeviceName() const;

	[[nodiscard]]
	bool hasExtension(const char* ext) const noexcept;

	[[nodiscard]]
	bool connected() const noexcept;

private:
	ALCdevice* device = nullptr;
};

} // namespace Blackthorn::Audio