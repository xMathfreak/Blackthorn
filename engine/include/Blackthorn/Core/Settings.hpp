#pragma once

#include <type_traits>
#include <utility>
#include <concepts>
#include <filesystem>
#include <flat_map>
#include <functional>
#include <string_view>
#include <vector>

#include <toml++/toml.hpp>

#include "Blackthorn/Core/Export.hpp"
#include "Blackthorn/Core/TOMLUtils.hpp"
#include "Blackthorn/Types/Numeric.hpp"

namespace Blackthorn::Core {

template <typename T>
concept StringLike =
    std::convertible_to<T, std::string_view> &&
    !std::same_as<std::remove_cvref_t<T>, bool>;

template <typename T>
concept SettingData =
	std::same_as<T, bool> ||
	std::is_arithmetic_v<T> ||
	StringLike<T>;

class BLACKTHORN_API Settings {
	using SettingChangedCallback = std::function<void(
		const toml::node*,
		const toml::node&
	)>;

public:
	[[nodiscard]]
	static Settings& instance();

	Settings(const Settings&) = delete;
	Settings& operator=(const Settings&) = delete;

	bool loadFromFile(const std::filesystem::path& file);
	bool saveToFile(const std::filesystem::path& file);

	template <SettingData T>
	[[nodiscard]]
	std::optional<T> get(std::string_view section, std::string_view key) const noexcept {
		const auto& node = data[section][key].node();

		if (!node)
			return std::nullopt;

		return fromTOMLValue<T>(*node);
	}

	template <SettingData T>
	[[nodiscard]]
	auto getOr(
		std::string_view section,
		std::string_view key,
		const T& fallback
	) {
		auto secIt = data.insert(section, toml::table{}).first;

		if (auto* sec = secIt->second.as_table()) {
			if (auto* existing = sec->get(key)) {
				if (auto value = fromTOMLValue<T>(*existing))
					return *value;

				return fallback;
			}

			insertDefaultTOMLValue(*sec, key, fallback);
			dirty = true;
		}

		return fallback;
}

	template <SettingData T>
	void set(
		std::string_view section,
		std::string_view key,
		const T& value
	) {
		auto it = data.insert(section, toml::table{}).first;

		if (auto* sec = it->second.as_table()) {
			auto* existing = sec->get(key);

			if (existing) {
				if (auto oldValue = fromTOMLValue<T>(*existing)) {
					if (*oldValue == value)
						return;
				}
			}

			toml::node* oldValue = sec->get(key);
			auto cloned = cloneNode(oldValue);

			insertTOMLValue(*sec, key, value);
			dirty = true;

			if (auto* updated = sec->get(key))
				fireCallbacks(section, key, cloned.get(), *updated);
		}
	}

	template <SettingData T>
	void setDefault(
		std::string_view section,
		std::string_view key,
		const T& value
	) {
		auto it = data.insert(section, toml::table{}).first;

		if (auto* sec = it->second.as_table()) {
			const auto existing = sec->contains(key);

			if (!existing) {
				insertDefaultTOMLValue(*sec, key, value);
				dirty = true;
			}
		}
	}

	void onChange(
		std::string_view section,
		std::string_view key,
		std::function<void()> callback
	) {
		onChangeCallbacks[makeCallbackKey(section, key)].emplace_back(
			[callback = std::move(callback)]
			(const toml::node*, const toml::node&) {
				callback();
			}
		);
	}

	template <SettingData T>
	void onChange(
		std::string_view section,
		std::string_view key,
		std::function<void(const T&)> callback
	) {
		onChangeCallbacks[makeCallbackKey(section, key)].emplace_back(
			[callback = std::move(callback)](
				const toml::node*,
				const toml::node& newNode
			) {
				if (auto value = fromTOMLValue<T>(newNode))
					callback(*value);
			}
		);
	}

	template <SettingData T, SettingData U>
	void onChange(
		std::string_view section,
		std::string_view key,
		std::function<void(const T&, const U&)> callback
	) {
		onChangeCallbacks[makeCallbackKey(section, key)].emplace_back(
			[callback = std::move(callback)](
				const toml::node* oldNode,
				const toml::node& newNode
			) {
				if (!oldNode)
					return;

				auto oldValue = fromTOMLValue<T>(*oldNode);
				auto newValue = fromTOMLValue<U>(newNode);

				if (oldValue && newValue)
					callback(*oldValue, *newValue);
			}
		);
	}

	void clear() noexcept;

	[[nodiscard]]
	bool hasSection(std::string_view section) const noexcept;

	[[nodiscard]]
	bool hasKey(std::string_view section, std::string_view key) const noexcept;

	bool remove(std::string_view section, std::string_view key) noexcept;

	bool removeSection(std::string_view section) noexcept;

	[[nodiscard]]
	bool isDirty() const noexcept;
private:
	Settings() = default;

	void fireCallbacks(std::string_view section, std::string_view key, const toml::node* oldValue, const toml::node& newValue) const;

	[[nodiscard]]
	std::string makeCallbackKey(std::string_view section, std::string_view key) const noexcept;

	toml::table data;
	std::flat_map<std::string, std::vector<SettingChangedCallback>> onChangeCallbacks;
	bool dirty = false;
};

} // namespace Blackthorn::Core