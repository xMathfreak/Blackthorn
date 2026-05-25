#include "Audio/Backend/AudioDevice.h"

#include <AL/alext.h>

#include "Audio/AudioException.h"

namespace Blackthorn::Audio {

AudioDevice::AudioDevice()
	: AudioDevice(nullptr)
{}

AudioDevice::AudioDevice(const char* name) {
	device = alcOpenDevice(name);

	if (!device)
		throw AudioException("Failed to open OpenAL device");
}

AudioDevice::~AudioDevice() {
	if (device)
		alcCloseDevice(device);
}

AudioDevice::AudioDevice(AudioDevice&& other) noexcept
	: device(other.device)
{
	other.device = nullptr;
}

AudioDevice& AudioDevice::operator=(AudioDevice&& other) noexcept {
	if (this == &other)
		return *this;

	if (device)
		alcCloseDevice(device);

	device = other.device;
	other.device = nullptr;

	return *this;
}

ALCdevice* AudioDevice::get() const noexcept {
	return device;
}

bool AudioDevice::valid() const noexcept {
	return device != nullptr;
}

std::string AudioDevice::getDeviceName() const {
	if (!device)
		return {};

	const ALCchar* name = alcGetString(device, ALC_DEVICE_SPECIFIER);

	return name ? name : "";
}

bool AudioDevice::hasExtension(const char* ext) const noexcept {
	return alcIsExtensionPresent(device, ext) == ALC_TRUE;
}

bool AudioDevice::connected() const noexcept {
	if (!device)
		return false;

	if (!hasExtension("ALC_EXT_disconnect"))
		return true;

	ALCint connected = ALC_TRUE;

	alcGetIntegerv(device, ALC_CONNECTED, 1, &connected);

	return (connected == ALC_TRUE);
}

} // namespace Blackthorn::Audio