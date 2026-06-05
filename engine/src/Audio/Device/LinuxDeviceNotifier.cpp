#if defined(__linux__)

#include "Audio/Device/LinuxDeviceNotifier.h"

#include <dlfcn.h>

#include "Debug/Logger.h"

extern "C" {

typedef struct pa_mainloop          pa_mainloop;
typedef struct pa_mainloop_api      pa_mainloop_api;
typedef struct pa_context           pa_context;
typedef struct pa_subscription_event_type pa_subscription_event_type_t;
typedef struct pa_operation         pa_operation;

typedef enum {
	PA_CONTEXT_UNCONNECTED,
	PA_CONTEXT_CONNECTING,
	PA_CONTEXT_AUTHORIZING,
	PA_CONTEXT_SETTING_NAME,
	PA_CONTEXT_READY,
	PA_CONTEXT_FAILED,
	PA_CONTEXT_TERMINATED
} pa_context_state_t;

static constexpr unsigned PA_SUBSCRIPTION_MASK_SINK = 0x0001U;

typedef void (*pa_context_notify_cb_t)(pa_context*, void*);
typedef void (*pa_context_subscribe_cb_t)(
	pa_context*,
	unsigned,
	unsigned,
	void*
);

using fn_pa_mainloop_new        = pa_mainloop*(*)();
using fn_pa_mainloop_get_api    = pa_mainloop_api*(*)(pa_mainloop*);
using fn_pa_mainloop_iterate    = int(*)(pa_mainloop*, int, int*);
using fn_pa_mainloop_free       = void(*)(pa_mainloop*);
using fn_pa_context_new         = pa_context*(*)(pa_mainloop_api*, const char*);
using fn_pa_context_connect     = int(*)(pa_context*, const char*, int, void*);
using fn_pa_context_get_state   = pa_context_state_t(*)(pa_context*);
using fn_pa_context_set_state_callback
	= void(*)(pa_context*, pa_context_notify_cb_t, void*);
using fn_pa_context_set_subscribe_callback
	= void(*)(pa_context*, pa_context_subscribe_cb_t, void*);
using fn_pa_context_subscribe    = pa_operation*(*)(pa_context*, unsigned, void*, void*);
using fn_pa_context_disconnect   = void(*)(pa_context*);
using fn_pa_context_unref        = void(*)(pa_context*);

} // extern "C"

namespace Blackthorn::Audio {

struct LinuxDeviceNotifier::Impl {
	// dlopen handle for libpulse
	void* pulseLib = nullptr;

	// Function pointers loaded from libpulse
	fn_pa_mainloop_new              pa_mainloop_new              = nullptr;
	fn_pa_mainloop_get_api          pa_mainloop_get_api          = nullptr;
	fn_pa_mainloop_iterate          pa_mainloop_iterate          = nullptr;
	fn_pa_mainloop_free             pa_mainloop_free             = nullptr;
	fn_pa_context_new               pa_context_new               = nullptr;
	fn_pa_context_connect           pa_context_connect           = nullptr;
	fn_pa_context_get_state         pa_context_get_state         = nullptr;
	fn_pa_context_set_state_callback
		pa_context_set_state_callback  = nullptr;
	fn_pa_context_set_subscribe_callback
		pa_context_set_subscribe_callback = nullptr;
	fn_pa_context_subscribe         pa_context_subscribe         = nullptr;
	fn_pa_context_disconnect        pa_context_disconnect        = nullptr;
	fn_pa_context_unref             pa_context_unref             = nullptr;

	// Runtime state
	pa_mainloop* mainloop = nullptr;
	pa_context*  context  = nullptr;

	std::thread       eventThread;
	std::atomic<bool> stopFlag { false };
};

LinuxDeviceNotifier::LinuxDeviceNotifier()
	: impl(std::make_unique<Impl>())
{}

LinuxDeviceNotifier::~LinuxDeviceNotifier() {
	stop();
}

void LinuxDeviceNotifier::setCallback(
	std::function<void(DeviceHint)> cb
) {
	callback = std::move(cb);
}

bool LinuxDeviceNotifier::isRunning() const noexcept {
	return running;
}

bool LinuxDeviceNotifier::start() {
	if (running)
		return true;

	// Try PipeWire first, then PulseAudio, then accept no-op.
	if (tryStartPipeWire()) {
		activeBackend = Backend::PipeWire;
		running       = true;
		BT_LOG("LinuxDeviceNotifier: using PipeWire backend");
		return true;
	}

	if (tryStartPulseAudio()) {
		activeBackend = Backend::PulseAudio;
		running       = true;
		BT_LOG("LinuxDeviceNotifier: using PulseAudio backend");
		return true;
	}

	// No notification backend available, fall back to ALC_CONNECTED polling.
	activeBackend = Backend::None;
	running       = true;
	BT_LOG(
		"LinuxDeviceNotifier: no PipeWire or PulseAudio available, "
		"relying on ALC_CONNECTED polling only"
	);
	return true;
}

void LinuxDeviceNotifier::stop() {
	if (!running)
		return;

	switch (activeBackend) {
		case Backend::PipeWire:   stopPipeWire();   break;
		case Backend::PulseAudio: stopPulseAudio(); break;
		case Backend::None:                         break;
	}

	activeBackend = Backend::None;
	running       = false;
}

bool LinuxDeviceNotifier::tryStartPipeWire() {
	// PipeWire provides a libpulse drop-in, so we probe for it by trying
	// to open libpipewire-0.3 and checking for the pulse compat symbols.
	void* pw = dlopen("libpipewire-0.3.so.0", RTLD_LAZY | RTLD_NOLOAD);

	if (!pw)
		return false;

	dlclose(pw);

	// PipeWire is present, its PulseAudio compatibility layer will be
	// picked up by tryStartPulseAudio() via libpulse.
	return false; // fall through to PulseAudio path deliberately
}

void LinuxDeviceNotifier::stopPipeWire() {
	// PipeWire is handled via the PulseAudio path.
	stopPulseAudio();
}

// ---------------------------------------------------------------------------
// PulseAudio backend
// ---------------------------------------------------------------------------

bool LinuxDeviceNotifier::tryStartPulseAudio() {
	// Try to load libpulse at runtime.
	impl->pulseLib = dlopen("libpulse.so.0", RTLD_LAZY);

	if (!impl->pulseLib) {
		BT_LOG(
			"LinuxDeviceNotifier: libpulse.so.0 not found ({})",
			dlerror()
		);
		return false;
	}

	// Load all required symbols.
#define LOAD_PA(name) \
	impl->name = reinterpret_cast<fn_##name>( \
		dlsym(impl->pulseLib, #name)); \
	if (!impl->name) { \
		BT_WARN("LinuxDeviceNotifier: missing symbol " #name); \
		dlclose(impl->pulseLib); \
		impl->pulseLib = nullptr; \
		return false; \
	}

	LOAD_PA(pa_mainloop_new)
	LOAD_PA(pa_mainloop_get_api)
	LOAD_PA(pa_mainloop_iterate)
	LOAD_PA(pa_mainloop_free)
	LOAD_PA(pa_context_new)
	LOAD_PA(pa_context_connect)
	LOAD_PA(pa_context_get_state)
	LOAD_PA(pa_context_set_state_callback)
	LOAD_PA(pa_context_set_subscribe_callback)
	LOAD_PA(pa_context_subscribe)
	LOAD_PA(pa_context_disconnect)
	LOAD_PA(pa_context_unref)

#undef LOAD_PA

	impl->mainloop = impl->pa_mainloop_new();

	if (!impl->mainloop) {
		BT_WARN("LinuxDeviceNotifier: pa_mainloop_new failed");
		dlclose(impl->pulseLib);
		impl->pulseLib = nullptr;
		return false;
	}

	pa_mainloop_api* api = impl->pa_mainloop_get_api(impl->mainloop);

	impl->context = impl->pa_context_new(api, "BlackthornDeviceNotifier");

	if (!impl->context) {
		BT_WARN("LinuxDeviceNotifier: pa_context_new failed");
		impl->pa_mainloop_free(impl->mainloop);
		impl->mainloop = nullptr;
		dlclose(impl->pulseLib);
		impl->pulseLib = nullptr;
		return false;
	}

	// Subscribe callback, fires when a sink changes state.
	impl->pa_context_set_subscribe_callback(
		impl->context,
		[](pa_context*, unsigned eventType, unsigned /*idx*/, void* ud) {
			auto* self = static_cast<LinuxDeviceNotifier*>(ud);
			// PA_SUBSCRIPTION_EVENT_SINK | PA_SUBSCRIPTION_EVENT_REMOVE = 0x11
			if ((eventType & 0xFF) == 0x11 && self->callback)
				self->callback(DeviceHint::DefaultDeviceChanged);
			// PA_SUBSCRIPTION_EVENT_SINK | PA_SUBSCRIPTION_EVENT_NEW = 0x01
			if ((eventType & 0xFF) == 0x01 && self->callback)
				self->callback(DeviceHint::DeviceArrived);
		},
		this
	);

	// State callback — subscribe once the context is ready.
	impl->pa_context_set_state_callback(
		impl->context,
		[](pa_context* ctx, void* ud) {
			auto* self  = static_cast<LinuxDeviceNotifier*>(ud);
			const auto s = self->impl->pa_context_get_state(ctx);
			if (s == PA_CONTEXT_READY) {
				self->impl->pa_context_subscribe(
					ctx,
					PA_SUBSCRIPTION_MASK_SINK,
					nullptr,
					nullptr
				);
			}
		},
		this
	);

	if (impl->pa_context_connect(
		impl->context, nullptr, 0, nullptr
	) < 0) {
		BT_WARN("LinuxDeviceNotifier: pa_context_connect failed");
		impl->pa_context_unref(impl->context);
		impl->context = nullptr;
		impl->pa_mainloop_free(impl->mainloop);
		impl->mainloop = nullptr;
		dlclose(impl->pulseLib);
		impl->pulseLib = nullptr;
		return false;
	}

	// Run the PulseAudio event loop on a background thread.
	impl->stopFlag.store(false, std::memory_order::relaxed);
	impl->eventThread = std::thread([this] {
		while (!impl->stopFlag.load(std::memory_order::relaxed)) {
			int retval = 0;
			impl->pa_mainloop_iterate(impl->mainloop, 0, &retval);
		}
	});

	return true;
}

void LinuxDeviceNotifier::stopPulseAudio() {
	if (impl->stopFlag.exchange(true, std::memory_order::acq_rel))
		return;

	if (impl->eventThread.joinable())
		impl->eventThread.join();

	if (impl->context) {
		impl->pa_context_disconnect(impl->context);
		impl->pa_context_unref(impl->context);
		impl->context = nullptr;
	}

	if (impl->mainloop) {
		impl->pa_mainloop_free(impl->mainloop);
		impl->mainloop = nullptr;
	}

	if (impl->pulseLib) {
		dlclose(impl->pulseLib);
		impl->pulseLib = nullptr;
	}
}

} // namespace Blackthorn::Audio

#endif // __linux__