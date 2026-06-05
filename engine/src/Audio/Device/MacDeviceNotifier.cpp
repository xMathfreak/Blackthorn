#ifdef __APPLE__

#include "Audio/Device/MacDeviceNotifier.h"

#include "Debug/Logger.h"

namespace Blackthorn::Audio {

MacDeviceNotifier::MacDeviceNotifier() = default;

MacDeviceNotifier::~MacDeviceNotifier() {
	stop();
}

void MacDeviceNotifier::setCallback(
	std::function<void(DeviceHint)> cb
) {
	callback = std::move(cb);
}

bool MacDeviceNotifier::start() {
	if (running)
		return true;

	const AudioObjectPropertyAddress addr = {
		kAudioHardwarePropertyDefaultOutputDevice,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMain
	};

	const OSStatus err = AudioObjectAddPropertyListener(
		kAudioObjectSystemObject,
		&addr,
		&MacDeviceNotifier::propertyListenerProc,
		this
	);

	if (err != noErr) {
		BT_ERROR(
			"MacDeviceNotifier: AudioObjectAddPropertyListener failed ({})",
			static_cast<int>(err)
		);
		return false;
	}

	running = true;
	BT_LOG("MacDeviceNotifier: started");
	return true;
}

void MacDeviceNotifier::stop() {
	if (!running)
		return;

	const AudioObjectPropertyAddress addr = {
		kAudioHardwarePropertyDefaultOutputDevice,
		kAudioObjectPropertyScopeGlobal,
		kAudioObjectPropertyElementMain
	};

	AudioObjectRemovePropertyListener(
		kAudioObjectSystemObject,
		&addr,
		&MacDeviceNotifier::propertyListenerProc,
		this
	);

	running = false;
	BT_LOG("MacDeviceNotifier: stopped");
}

bool MacDeviceNotifier::isRunning() const noexcept {
	return running;
}

OSStatus MacDeviceNotifier::propertyListenerProc(
	AudioObjectID /*inObjectID*/,
	UInt32 /*inNumberAddresses*/,
	const AudioObjectPropertyAddress* /*inAddresses*/,
	void* inClientData
) {
	auto* self = static_cast<MacDeviceNotifier*>(inClientData);

	if (self && self->callback)
		self->callback(DeviceHint::DefaultDeviceChanged);

	return noErr;
}

} // namespace Blackthorn::Audio

#endif // __APPLE__