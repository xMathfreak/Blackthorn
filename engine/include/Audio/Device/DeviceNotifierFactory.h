#pragma once

#include <memory>

#include "Audio/Device/IDeviceNotifier.h"
#include "Core/Export.h"

namespace Blackthorn::Audio {

/**
 * @brief Creates the appropriate @c IDeviceNotifier for the current platform.
 *
 * | Platform | Implementation            |
 * |----------|---------------------------|
 * | Windows  | @c WindowsDeviceNotifier  |
 * | macOS    | @c MacDeviceNotifier      |
 * | Linux    | @c LinuxDeviceNotifier    |
 *
 * Returns a valid (non-null) notifier on all platforms. On Linux, if
 * neither PipeWire nor PulseAudio is available, the returned notifier
 * starts successfully but fires no callbacks — the audio thread's
 * @c ALC_CONNECTED polling handles detection alone.
 */
class BLACKTHORN_API DeviceNotifierFactory {
public:
	DeviceNotifierFactory() = delete;

	/**
	 * @brief Constructs the platform-appropriate @c IDeviceNotifier.
	 *
	 * Does not call @c start(). The caller is responsible for setting a
	 * callback and starting the notifier before use.
	 *
	 * @return A non-null @c IDeviceNotifier for the current platform.
	 */
	[[nodiscard]]
	static std::unique_ptr<IDeviceNotifier> create();
};

} // namespace Blackthorn::Audio