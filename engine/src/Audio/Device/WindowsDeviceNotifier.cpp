#ifdef _WIN32

#include "Audio/Device/WindowsDeviceNotifier.h"

#include <objbase.h>

#include "Debug/Logger.h"

namespace Blackthorn::Audio {

WindowsDeviceNotifier::WindowsDeviceNotifier() = default;

WindowsDeviceNotifier::~WindowsDeviceNotifier() {
	stop();
}

void WindowsDeviceNotifier::setCallback(
	std::function<void(DeviceHint)> cb
) {
	callback = std::move(cb);
}

bool WindowsDeviceNotifier::start() {
	if (running)
		return true;

	const HRESULT hr = CoInitializeEx(
		nullptr,
		COINIT_APARTMENTTHREADED
	);

	if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
		BT_ERROR(
			"WindowsDeviceNotifier: CoInitializeEx failed (0x{:X})",
			static_cast<unsigned>(hr)
		);
		return false;
	}

	HRESULT enumHr = CoCreateInstance(
		__uuidof(MMDeviceEnumerator),
		nullptr,
		CLSCTX_ALL,
		__uuidof(IMMDeviceEnumerator),
		reinterpret_cast<void**>(&enumerator)
	);

	if (FAILED(enumHr)) {
		BT_ERROR(
			"WindowsDeviceNotifier: Failed to create IMMDeviceEnumerator "
			"(0x{:X})", static_cast<unsigned>(enumHr)
		);
		CoUninitialize();
		return false;
	}

	const HRESULT regHr = enumerator->RegisterEndpointNotificationCallback(this);

	if (FAILED(regHr)) {
		BT_ERROR(
			"WindowsDeviceNotifier: RegisterEndpointNotificationCallback "
			"failed (0x{:X})", static_cast<unsigned>(regHr)
		);
		enumerator->Release();
		enumerator = nullptr;
		CoUninitialize();
		return false;
	}

	running = true;
	BT_LOG("WindowsDeviceNotifier: started");
	return true;
}

void WindowsDeviceNotifier::stop() {
	if (!running)
		return;

	if (enumerator) {
		enumerator->UnregisterEndpointNotificationCallback(this);
		enumerator->Release();
		enumerator = nullptr;
	}

	CoUninitialize();
	running = false;
	BT_LOG("WindowsDeviceNotifier: stopped");
}

bool WindowsDeviceNotifier::isRunning() const noexcept {
	return running;
}

HRESULT STDMETHODCALLTYPE
WindowsDeviceNotifier::OnDefaultDeviceChanged(
	EDataFlow flow,
	ERole   role,
	LPCWSTR  /*pwstrDefaultDeviceId*/
) {
	if (flow == eRender && role == eConsole && callback)
		callback(DeviceHint::DefaultDeviceChanged);

	return S_OK;
}

HRESULT STDMETHODCALLTYPE
WindowsDeviceNotifier::OnDeviceAdded(LPCWSTR /*pwstrDeviceId*/) {
	if (callback)
		callback(DeviceHint::DeviceArrived);

	return S_OK;
}

HRESULT STDMETHODCALLTYPE
WindowsDeviceNotifier::OnDeviceRemoved(LPCWSTR /*pwstrDeviceId*/) {
	if (callback)
		callback(DeviceHint::DefaultDeviceChanged);

	return S_OK;
}

HRESULT STDMETHODCALLTYPE
WindowsDeviceNotifier::OnDeviceStateChanged(
	LPCWSTR /*pwstrDeviceId*/,
	DWORD  dwNewState
) {
	if (dwNewState == DEVICE_STATE_ACTIVE && callback)
		callback(DeviceHint::DeviceArrived);

	return S_OK;
}

HRESULT STDMETHODCALLTYPE
WindowsDeviceNotifier::OnPropertyValueChanged(
	LPCWSTR     /*pwstrDeviceId*/,
	const PROPERTYKEY /*key*/
) {
	return S_OK;
}

HRESULT STDMETHODCALLTYPE
WindowsDeviceNotifier::QueryInterface(
	REFIID riid,
	void** ppvObject
) {
	if (riid == __uuidof(IUnknown) ||
		riid == __uuidof(IMMNotificationClient))
	{
		*ppvObject = static_cast<IMMNotificationClient*>(this);
		AddRef();
		return S_OK;
	}

	*ppvObject = nullptr;
	return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE WindowsDeviceNotifier::AddRef() {
	return refCount.fetch_add(1, std::memory_order::relaxed) + 1;
}

ULONG STDMETHODCALLTYPE WindowsDeviceNotifier::Release() {
	const ULONG count =
		refCount.fetch_sub(1, std::memory_order::acq_rel) - 1;

	return count;
}

} // namespace Blackthorn::Audio

#endif // _WIN32