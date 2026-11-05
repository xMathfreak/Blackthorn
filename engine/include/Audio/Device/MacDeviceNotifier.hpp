#pragma once

#ifdef __APPLE__

#include <functional>

#include <CoreAudio/CoreAudio.h>

#include "Audio/Device/IDeviceNotifier.h"
#include "Core/Export.h"

namespace Blackthorn::Audio {

/**
 * @brief macOS device change notifier via CoreAudio property listeners.
 *
 * Registers an @c AudioObjectPropertyListener on
 * @c kAudioObjectSystemObject for @c kAudioHardwarePropertyDefaultOutputDevice.
 * The listener fires on the CoreAudio thread whenever the system default
 * output device changes (e.g. AirPods connect, headphones unplugged).
 *
 * @section threading Thread safety
 * CoreAudio fires property listeners on a private high-priority thread.
 * The user-supplied callback must be thread-safe and must not block.
 */
class BLACKTHORN_API MacDeviceNotifier final : public IDeviceNotifier {
public:
	MacDeviceNotifier();
	~MacDeviceNotifier() override;

	void setCallback(
		std::function<void(DeviceHint)> callback
	) override;

	bool start() override;
	void stop() override;

	[[nodiscard]]
	bool isRunning() const noexcept override;

private:
	static OSStatus propertyListenerProc(
		AudioObjectID inObjectID,
		UInt32 inNumberAddresses,
		const AudioObjectPropertyAddress* inAddresses,
		void* inClientData
	);

private:
	std::function<void(DeviceHint)> callback;
	bool running = false;
};

} // namespace Blackthorn::Audio

#endif // __APPLE__