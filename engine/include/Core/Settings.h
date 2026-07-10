#pragma once

#include <algorithm>
#include <charconv>
#include <concepts>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Core/Export.h"

namespace Blackthorn::Core {

template <typename T>
concept SupportedSettingType =
	std::same_as<T, bool> ||
	std::integral<T> ||
	std::floating_point<T> ||
	std::convertible_to<T, std::string>;

/**
 * @brief Thread-safe singleton for INI-style key/value settings.
 *
 * Provides a centralized configuration system with automatic parsing,
 * case-insensitive lookup, and lazy key initialization.
 *
 * @section case_insensitivity Case Insensitivity
 * Section and key names are case-insensitive. For example, "Vsync",
 * "vsync", and "VSYNC" all resolve to the same entry.
 *
 * Names are normalized to lowercase on every read, write, and parse,
 * ensuring that both the in-memory representation and saved file remain
 * consistent.
 *
 * @section auto_create Auto-create Behavior
 * Calling `get()` for a key that does not exist will:
 *   - Insert the default value into the in-memory store
 *   - Return that value to the caller
 *
 * This means the first read of any key automatically bootstraps a valid
 * entry. Calling saveToFile() afterward will persist all accessed keys.
 *
 * @section usage Typical Usage
 * @code
 * auto& cfg = Settings::instance();
 * cfg.loadFromFile("settings.ini");
 *
 * bool fs   = cfg.get<bool>("Window", "Fullscreen");
 * int  w    = cfg.get<int>("Window", "Width", 1280);
 * float vol = cfg.get<float>("Audio", "MasterVolume", 1.0f);
 *
 * cfg.set("Window", "Width", 1920);
 * cfg.saveToFile("settings.ini");
 * @endcode
 */
class BLACKTHORN_API Settings {
public:
	static Settings& instance();

	Settings(const Settings&) = delete;
	Settings& operator=(const Settings&) = delete;

	/**
	 * @brief Loads settings from an INI file, replacing all in-memory state.
	 *
	 * Lines beginning with '#' or ';' (after trimming) are comments.
	 * Whitespace is stripped from section names, keys, and values.
	 * Casing is preserved exactly as it appears in the file.
	 *
	 * @param path  Path to the INI file.
	 * @return true if the file was opened and read successfully.
	 */
	bool loadFromFile(const std::filesystem::path& path);

	/**
	 * @brief Saves all in-memory settings to an INI file.
	 *
	 * Sections and keys are written in insertion order.
	 *
	 * @param path  Path to write. Created if absent, overwritten if present.
	 * @return true on success.
	 */
	bool saveToFile(const std::filesystem::path& path);

	/**
	 * @brief Returns the value of a key, creating it with a default if absent.
	 *
	 * Lookup is case-insensitive. Section and key names are normalized to
	 * lowercase before any map operation.
	 *
	 * @tparam T       Value type (bool, integral, float, double, std::string).
	 * @param section  Section name.
	 * @param key      Key name.
	 * @param fallback Value used when the key is absent or its stored string
	 *                 cannot be parsed. Defaults to `T{}` if omitted.
	 * @return T       The resolved value.
	 */
	template <SupportedSettingType T>
	T get(const std::string& section, const std::string& key, std::optional<T> fallback = std::nullopt) {
		const std::string sec = this->normalize(section);
		const std::string k = this->normalize(key);
		const T def = fallback.value_or(T{});

		std::string rawValue;
		bool found = false;

		{
			std::lock_guard lock(mutex);
			ensureSectionOrder(sec);

			auto& sd = groupedValues[sec];
			auto it = sd.values.find(k);

			if (it != sd.values.end()) {
				rawValue = it->second;
				found = true;
			}
		}

		if (found) {
			if constexpr (std::is_same_v<T, bool>) {
				const auto v = toLower(rawValue);

				if (v == "true" || v == "1" || v == "yes" || v == "on")
					return true;

				if (v == "false"|| v == "0" || v == "no" || v == "off")
					return false;
			} else if constexpr (std::is_integral_v<T>) {
				if (auto parsedInt = parseInt<T>(rawValue))
					return *parsedInt;
			} else if constexpr (std::is_floating_point_v<T>) {
				if (auto parsedFloat = parseFloat<T>(rawValue))
					return *parsedFloat;
			} else if constexpr (std::is_convertible_v<T, std::string>) {
				return rawValue;
			} else {
				static_assert(Detail::dependentFalse<T>, "Unsupported type for Settings::get");
			}
		}

		writeRaw(sec, k, serialize(def));
		return def;
	}

	/**
	 * @brief Sets the value of a key, creating the section/key if absent.
	 *
	 * @tparam T Value type.
	 * @param section Section name (case-insensitive, whitespace trimmed).
	 * @param key Key name (case-insensitive, whitespace trimmed).
	 * @param value Value to store.
	 */
	template <SupportedSettingType T>
	void set(const std::string& section, const std::string& key, const T& value) {
		writeRaw(section, key, serialize(value));
	}

	/**
	 * @brief Writes a default value only if the key does not exist.
	 */
	template <SupportedSettingType T>
	void setDefault(const std::string& section, const std::string& key, const T& value) {
		const std::string sec = normalize(section);
		const std::string k = normalize(key);

		{
			std::lock_guard lock(mutex);
			auto secIt = groupedValues.find(sec);

			if (secIt != groupedValues.end() && secIt->second.values.count(k))
				return;
		}

		writeRaw(sec, k, serialize(value));
	}

	/**
	 * @brief Registers a callback for when a key's value changes.
	 *
	 * @param section Section name (case-insensitive).
	 * @param key Key name (case insensitive).
	 * @param callback Called with the new raw string value on each `set()`.
	 */
	void onChange(const std::string& section, const std::string& key, std::function<void(const std::string&)> callback);

	/** Returns true if the section exists in memory (case-insensitive). */
	bool hasSection(const std::string& section) const;

	/** Returns true if the key exists under section (case-insensitive). */
	bool hasKey(const std::string& section, const std::string& key) const;

	/** Removes a single key. No-op if not present. */
	void remove(const std::string& section, const std::string& key);

	/** Removes an entire section and all its keys. No-op if not present. */
	void removeSection(const std::string& section);

	/** Clears all in-memory settings. */
	void clear();

	/**
	 * @brief Returns true if any value has been modified since the
	 * last `saveToFile()` or `markClean()` call.
	 *
	 * @return True if dirty, False otherwise.
	 */
	bool isDirty() const;

	/** Resets the dirty flag without saving */
	void markClean();

private:
	Settings() = default;

	struct SectionData {
		std::unordered_map<std::string, std::string> values;
		std::vector<std::string> keyOrder;
	};

	using ChangeCallback = std::function<void(const std::string&)>;

	// Key: normalize(secton) + "|" + normalize(key)
	std::unordered_map<std::string, std::vector<ChangeCallback>> changeCallbacks;

	std::unordered_map<std::string, SectionData> groupedValues;
	std::vector<std::string> sectionOrder;
	mutable std::recursive_mutex mutex;
	bool dirty = false;

	void writeRaw(const std::string& sec, const std::string& k, const std::string& rawValue) {
		bool changed = false;

		{
			std::lock_guard lock(mutex);
			ensureSectionOrder(sec);

			auto& sd = groupedValues[sec];
			auto it = sd.values.find(k);

			if (it == sd.values.end()) {
				sd.keyOrder.push_back(k);
				sd.values[k] = rawValue;
				dirty = true;
				changed = true;
			} else if (it->second != rawValue) {
				it->second = rawValue;
				dirty = true;
				changed = true;
			}
		}

		if (changed) {
			const std::string cbKey = sec + "|" + k;
			std::vector<std::function<void(const std::string&)>> toFire;

			{
				std::lock_guard<std::recursive_mutex> lock(mutex);
				auto it = changeCallbacks.find(cbKey);
				if (it != changeCallbacks.end())
					toFire = it->second;
			}

			for (auto& cb : toFire)
				cb(rawValue);
		}
	}

	void ensureSectionOrder(const std::string& sec) {
		for (const auto& s : sectionOrder) {
			if (s == sec)
				return;
		}

		sectionOrder.push_back(sec);
	}

	/// Strips leading/trailing whitespace and lowercases
	static std::string normalize(std::string s) {
		const auto notSpace = [](unsigned char c){ return !std::isspace(c); };
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
		s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
		std::transform(s.begin(), s.end(), s.begin(),
			[](unsigned char c){ return static_cast<char>(std::tolower(c)); });
		return s;
	}

	/// Lowercases a copy
	static std::string toLower(std::string s) {
		std::transform(s.begin(), s.end(), s.begin(),
			[](unsigned char c){ return static_cast<char>(std::tolower(c)); });

		return s;
	}

	template <typename T>
	requires std::is_integral_v<T> && (!std::is_same_v<T, bool>)
	static std::optional<T> parseInt(const std::string& s) {
		T result{};
		auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), result);

		if (ec == std::errc{} && ptr == s.data() + s.size())
			return result;

		return std::nullopt;
	}

	template <typename T>
	requires std::is_floating_point_v<T>
	static std::optional<T> parseFloat(const std::string& s) {
		T result{};
		const char* begin = s.data();
		const char* end = s.data() + s.size();

		auto [ptr, ec] = std::from_chars(begin, end, result, std::chars_format::general);

		if (ec == std::errc{} && ptr == end)
			return result;

		return std::nullopt;
	}

	template <typename T>
	static std::string serialize(const T& value) {
		if constexpr (std::is_same_v<T, bool>) {
			return value ? "true" : "false";
		} else if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>) {
			return std::to_string(value);
		} else if constexpr (std::is_convertible_v<T, std::string>) {
			return std::string(value);
		} else {
			static_assert(Detail::dependentFalse<T>, "Settings::serialize: unsupported type");
		}
	}

	struct Detail {
		template <typename> static constexpr bool dependentFalse = false;
	};
};

} // namespace Blackthorn::Core