#pragma once

#include <functional>

#include "Core/Export.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::Audio {

/**
 * @brief Hint events fired by @c IDeviceNotifier implementations.
 *
 * These are hints, not authoritative state. The audio thread always
 * validates device connectivity via @c ALC_CONNECTED polling before
 * taking action. The notifier simply reduces the latency between a
 * hardware event and the audio thread becoming aware of it.
 */
enum class DeviceHint : U8 {
	/// The default output device may have changed or become unavailable.
	/// The audio thread should re-evaluate @c ALC_CONNECTED immediately.
	DefaultDeviceChanged,

	/// A new audio output device has become available. The audio thread
	/// should attempt reconnection if it is currently in the lost state.
	DeviceArrived,
};

/**
 * @brief Platform-agnostic interface for receiving audio device change hints.
 *
 * Implementations subscribe to OS-level device notifications and translate
 * them into @c DeviceHint callbacks. The callback is fired from whatever
 * thread the OS delivers the notification on — it must be thread-safe.
 *
 * @section fallback ALC_CONNECTED fallback
 * @c IDeviceNotifier is a hint system only. The audio thread always polls
 * @c ALC_CONNECTED each tick as a safety net, regardless of whether any
 * hint has been received.
 *
 * @section platforms Platform implementations
 * | Platform | Mechanism                                              |
 * |----------|--------------------------------------------------------|
 * | Windows  | @c IMMNotificationClient via WASAPI                    |
 * | macOS    | @c AudioObjectAddPropertyListener (CoreAudio)          |
 * | Linux    | PipeWire subscription → PulseAudio subscription → none|
 *
 * @code
 * auto notifier = DeviceNotifierFactory::create();
 * notifier->setCallback([this](DeviceHint hint) {
 *     deviceHintFlag.store(true, std::memory_order::release);
 * });
 * notifier->start();
 * @endcode
 */
class BLACKTHORN_API IDeviceNotifier {
public:
	virtual ~IDeviceNotifier() = default;

	/**
	 * @brief Sets the callback invoked when a device hint is received.
	 *
	 * Must be called before @c start(). The callback may be invoked from
	 * any thread; it must not block or call back into @c AudioThread.
	 *
	 * @param callback Function to call with the hint type.
	 */
	virtual void setCallback(
		std::function<void(DeviceHint)> callback
	) = 0;

	/**
	 * @brief Starts listening for device notifications.
	 *
	 * @return true if the subscription was established successfully.
	 *         Returning false is non-fatal — the audio thread falls back
	 *         to @c ALC_CONNECTED polling alone.
	 */
	virtual bool start() = 0;

	/** @brief Stops listening and releases OS resources. */
	virtual void stop() = 0;

	/** @brief Returns true if the notifier is currently active. */
	[[nodiscard]]
	virtual bool isRunning() const noexcept = 0;
};

} // namespace Blackthorn::Audio