#pragma once

#include <atomic>
#include <concepts>
#include <string>

#include "Core/Settings.h"

namespace Blackthorn::Core {

/**
 * @brief Subset of SupportedSettingType that can be stored in a std::atomic.
 *
 * std::string is deliberately excluded - std::atomic<std::string> isn't a
 * thing, and a hot-path setting that's a string is unusual enough that it
 * should be cached by hand rather than forced through this primitive.
 */
template <typename T>
concept AtomicSettingType = std::same_as<T, bool> || std::integral<T> || std::floating_point<T>;

/**
 * @brief Mirrors a single Settings value in a lock-free, allocation-free
 * local cache, for use in hot loops that would otherwise pay a mutex lock
 * and string-normalization cost on every Settings::get<T>() call.
 *
 * @details
 * Construction is cheap and does not touch Settings at all - it only
 * stores the section/key/fallback. The actual sync with Settings happens
 * in attach(), which must be called once the settings file has been
 * loaded (Settings::loadFromFile() writes directly into its internal map
 * and does *not* fire onChange callbacks, so a CachedSetting attached
 * before the load would never observe values that came from the file).
 * In practice this means calling attach() from an EngineCore/Engine
 * override of registerEngineCallbacks(), alongside other
 * Settings::onChange registrations - exactly where this class's own
 * instances do so.
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
 *         EngineCore::registerEngineCallbacks(s);
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
 * @note Settings::onChange has no matching "unregister" API, and the
 * registered callback captures `this`. CachedSetting is therefore
 * non-copyable and non-movable, and is only safe to use for values with
 * engine/process lifetime (e.g. a member of Engine/EngineCore) that is
 * attach()'d exactly once - not something constructed and destroyed
 * per-frame, per-scene, or re-attached across a shutdown/re-init cycle.
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
	 * onChange callback that keeps the cache in sync from then on.
	 *
	 * Must be called once, after Settings::loadFromFile() has already run
	 * (see class-level note). Calling it more than once on the same
	 * instance registers a duplicate callback - not guarded against,
	 * since it's intended to run exactly once during initialization.
	 */
	void attach() {
		value.store(Settings::instance().get<T>(section, key, fallback), std::memory_order_relaxed);

		Settings::instance().onChange(section, key, [this](const std::string&) {
			// Re-fetch through the typed getter rather than parsing the raw
			// string ourselves - by the time onChange fires, Settings has
			// already committed the new value, so this returns the same
			// parsed/normalized result Settings::get<T>() would anywhere else.
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
