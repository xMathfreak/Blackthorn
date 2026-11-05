#pragma once

#include <atomic>
#include <concepts>
#include <string>

#include "Core/Settings.h"

namespace Blackthorn::Core {

/**
 * @brief Subset of SupportedSettingType that can be stored in a std::atomic.
 *
 * Cache std::string by other means, std::atomic<std::string> does not exsit.
 */
template <typename T>
concept AtomicSettingType = std::same_as<T, bool> || std::integral<T> || std::floating_point<T>;

/**
 * @brief Mirrors a single Settings value in a lock-free, allocation-free
 * local cache, for use in hot loops that would otherwise pay a mutex lock
 * and string-normalization cost on every Settings::get<T>() call.
 *
 * @details
 * Construction is cheap and does not access @c Settings. It only stores
 * the section, key and fallback value. The actual synchronization with
 * @c Settings occurs in attach(), which must be called after the settings
 * file has been loaded. Settings::loadFromFile() writes directly to its
 * internal map and does not fire `onChange` callbacks, so a CachedSetting
 * attached before the load would not observe values loaded from the file.
 *
 * In practice, attach should be called from a @c Runtime or @c Engine override
 * of registerEngineCallbacks(), alongisde the other Settings::onChange registrations.
 * This is also where the CachedSetting instances owned by those classes attach themselves.
 *
 * After attach(), get() is a single relaxed atomic load: no mutex, no
 * string allocation, no map lookup. Safe to call from any thread.
 *
 * @tparam T An AtomicSettingType (bool, integral, or floating-point).
 *
 * @section usage Usage
 * @code
 * class Engine {
 *     Core::CachedSetting<bool> vsyncEnabled{"window", "vsync", true};
 *     Core::CachedSetting<bool> frameCapEnabled{"graphics", "frame_cap", false};
 *     Core::CachedSetting<int> targetFPS{"graphics", "target_fps", 60};
 *
 *     void registerEngineCallbacks(Core::Settings& s) override {
 *         Runtime::registerEngineCallbacks(s);
 *         vsyncEnabled.attach();
 *         frameCapEnabled.attach();
 *         targetFPS.attach();
 *     }
 *
 *     void run() {
 *         if (frameCapEnabled.get() && !vsyncEnabled.get()) {
 *             const int fps = targetFPS.get(); // atomic load, no lock
 *             // ...
 *         }
 *     }
 * };
 * @endcode
 *
 * @note Settings::onChange has no matching unregister API, and the
 * registered callback captures `this`. CachedSetting is therefore non-copyable
 * and non-movable and is only safe to use for values with engine/process lifetime.
 *
 * A CachedSetting must attach() exactly once and must remain alive for
 * as long as its callback may be infoked. It not be constructed and destroyed
 * per-frame or per-scene, nor reattached across a shotdown/reinit cycle.
 */
template <AtomicSettingType T>
class CachedSetting {
public:
	CachedSetting(std::string sec, std::string k, T fall = T{})
		: section(std::move(sec))
		, key(std::move(k))
		, fallback(fall)
		, value(fall)
	{}

	CachedSetting(const CachedSetting&) = delete;
	CachedSetting& operator=(const CachedSetting&) = delete;
	CachedSetting(CachedSetting&&) = delete;
	CachedSetting& operator=(CachedSetting&&) = delete;

	/**
	 * @brief Performs the initial read from Settings and registers an
	 * onChange callback that keeps the cache synchronized thereafter.
	 *
	 * Must be called exactly once, after Settings::loadFromFile() has
	 * completed (see the class-level note). Calling attach() more than once
	 * on the same instance registers an additional callback; this is
	 * intentionally not guarded against because attach() is intended to be
	 * called exactly once during initialization.
	 */
	void attach() {
		value.store(Settings::instance().get<T>(section, key, fallback), std::memory_order_relaxed);

		Settings::instance().onChange(section, key, [this](const std::string&) {
			value.store(Settings::instance().get<T>(this->section, this->key), std::memory_order_relaxed);
		});
	}

	/// Returns the cached value. Lock-free; safe to call from a hot loop.
	T get() const noexcept { return value.load(std::memory_order_relaxed); }

	/// Implicit conversion, for use as a drop-in replacement of a plain T.
	operator T() const noexcept { return get(); }

private:
	std::string section;
	std::string key;
	T fallback;
	std::atomic<T> value;
};

} // namespace Blackthorn::Core
