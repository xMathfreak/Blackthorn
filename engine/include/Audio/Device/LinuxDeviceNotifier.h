#pragma once

#if defined(__linux__)

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

#include "Audio/Device/IDeviceNotifier.h"
#include "Core/Export.h"

namespace Blackthorn::Audio {

/**
 * @brief Linux device change notifier with a PipeWire → PulseAudio → no-op
 *        cascade.
 *
 * Attempts to subscribe to device notifications via the best available
 * mechanism at runtime:
 *
 * 1. **PipeWire** — if @c libpipewire-0.3 is available and the PipeWire
 *    daemon is running, subscribes to @c PW_TYPE_INTERFACE_Node removal
 *    events for sink nodes.
 * 2. **PulseAudio** — if @c libpulse is available and the PulseAudio daemon
 *    is running (or if PipeWire is acting as a PulseAudio drop-in), subscribes
 *    to @c PA_SUBSCRIPTION_MASK_SINK events.
 * 3. **No-op** — if neither is available, the notifier starts successfully
 *    but fires no callbacks. The audio thread's @c ALC_CONNECTED polling
 *    acts as the sole detection mechanism.
 *
 * @section dynamic_loading Dynamic loading
 * Both @c libpipewire and @c libpulse are loaded via @c dlopen at runtime
 * rather than linked at build time. This means the engine binary runs on
 * systems that have neither library installed, falling back to polling.
 *
 * @section threading Thread safety
 * PipeWire and PulseAudio callbacks fire on their respective event loop
 * threads. The user-supplied callback must be thread-safe.
 */
class BLACKTHORN_API LinuxDeviceNotifier final : public IDeviceNotifier {
public:
	LinuxDeviceNotifier();
	~LinuxDeviceNotifier() override;

	void setCallback(
		std::function<void(DeviceHint)> callback
	) override;

	bool start() override;
	void stop() override;

	[[nodiscard]]
	bool isRunning() const noexcept override;

private:
	enum class Backend {
		None,
		PipeWire,
		PulseAudio,
	};

	bool tryStartPipeWire();
	bool tryStartPulseAudio();
	void stopPipeWire();
	void stopPulseAudio();

private:
	std::function<void(DeviceHint)> callback;

	Backend activeBackend = Backend::None;
	bool running = false;

	// Opaque handles for the active backend.
	// Defined in the .cpp to avoid exposing PipeWire/PulseAudio headers.
	struct Impl;
	std::unique_ptr<Impl> impl;
};

} // namespace Blackthorn::Audio

#endif // __linux__