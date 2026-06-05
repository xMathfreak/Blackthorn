#include "Audio/Device/DeviceNotifierFactory.h"

#if defined(_WIN32)
	#include "Audio/Device/WindowsDeviceNotifier.h"
#elif defined(__APPLE__)
	#include "Audio/Device/MacDeviceNotifier.h"
#elif defined(__linux__)
	#include "Audio/Device/LinuxDeviceNotifier.h"
#endif

namespace Blackthorn::Audio {

std::unique_ptr<IDeviceNotifier> DeviceNotifierFactory::create() {
#if defined(_WIN32)
	return std::make_unique<WindowsDeviceNotifier>();
#elif defined(__APPLE__)
	return std::make_unique<MacDeviceNotifier>();
#elif defined(__linux__)
	return std::make_unique<LinuxDeviceNotifier>();
#else
	// Unsupported platform
	class NullNotifier final : public IDeviceNotifier {
	public:
		void setCallback(std::function<void(DeviceHint)>) override {}
		bool start() override { running = true; return true; }
		void stop() override { running = false; }
		bool isRunning() const noexcept override { return running; }
	private:
		bool running = false;
	};
	return std::make_unique<NullNotifier>();
#endif
}

} // namespace Blackthorn::Audio