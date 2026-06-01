#pragma once

#ifdef _WIN32

#include <atomic>
#include <functional>

#include <mmdeviceapi.h>

#include "Audio/Device/IDeviceNotifier.h"
#include "Core/Export.h"

namespace Blackthorn::Audio {

/**
 * @brief Windows device change notifier via WASAPI @c IMMNotificationClient.
 *
 * Subscribes to the Windows Multimedia Device API's default device change
 * notifications. The @c OnDefaultDeviceChanged callback is invoked by the
 * Windows audio engine on a dedicated COM thread whenever the user changes
 * the default output device (e.g. plugs or unplugs headphones).
 *
 * @section com COM initialisation
 * Windows requires COM to be initialised on the thread that creates the
 * @c IMMDeviceEnumerator. @c start() calls @c CoInitializeEx internally
 * and @c stop() calls @c CoUninitialize. The notifier owns its COM context.
 *
 * @section thread_safety Thread safety
 * The @c IMMNotificationClient callbacks fire on a Windows audio thread.
 * The user-supplied callback must be thread-safe. The notifier itself
 * only writes to the ref count (managed by COM) and calls the callback.
 */
class BLACKTHORN_API WindowsDeviceNotifier final
	: public IDeviceNotifier
	, public IMMNotificationClient
{
public:
	WindowsDeviceNotifier();
	~WindowsDeviceNotifier() override;

	void setCallback(
		std::function<void(DeviceHint)> callback
	) override;

	bool start() override;
	void stop() override;

	[[nodiscard]]
	bool isRunning() const noexcept override;

	HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(
		EDataFlow flow,
		ERole role,
		LPCWSTR pwstrDefaultDeviceId
	) override;

	HRESULT STDMETHODCALLTYPE OnDeviceAdded(
		LPCWSTR pwstrDeviceId
	) override;

	HRESULT STDMETHODCALLTYPE OnDeviceRemoved(
		LPCWSTR pwstrDeviceId
	) override;

	HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(
		LPCWSTR pwstrDeviceId,
		DWORD dwNewState
	) override;

	HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(
		LPCWSTR pwstrDeviceId,
		const PROPERTYKEY key
	) override;

	HRESULT STDMETHODCALLTYPE QueryInterface(
		REFIID riid,
		void** ppvObject
	) override;

	ULONG STDMETHODCALLTYPE AddRef() override;
	ULONG STDMETHODCALLTYPE Release() override;

private:
	std::function<void(DeviceHint)> callback;

	IMMDeviceEnumerator* enumerator = nullptr;

	std::atomic<ULONG> refCount { 1 };
	bool running = false;
};

} // namespace Blackthorn::Audio

#endif // _WIN32