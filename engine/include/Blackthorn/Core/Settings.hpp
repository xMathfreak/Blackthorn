#pragma once

#include <concepts>
#include <filesystem>
#include <flat_map>
#include <functional>
#include <string_view>
#include <vector>

#include <toml++/toml.hpp>

#include "Blackthorn/Core/Export.hpp"

namespace Blackthorn::Core {

template <typename T>
concept StringLike =
	std::is_convertible_v<T, std::string_view> &&
	!std::is_same_v<std::remove_cvref_t<T>, bool>;

template <typename T>
concept SettingData =
	std::same_as<T, bool> ||
	std::is_arithmetic_v<T> ||
	StringLike<T>;

template <typename T>
constexpr auto map_to_toml_scalar_type() {
	using U = std::decay_t<T>;

	if constexpr (std::is_same_v<U, bool>) {
		return bool{};
	} else if constexpr (std::is_integral_v<U>) {
		// Note: uint64_t values > INT64_MAX will overflow int64_t
		return int64_t{};
	} else if constexpr (std::is_floating_point_v<U>) {
		return double{};
	} else if constexpr (std::is_convertible_v<U, std::string_view>) {
		return std::string{};
	} else if constexpr (std::is_same_v<U, toml::date> ||
						 std::is_same_v<U, toml::time> ||
						 std::is_same_v<U, toml::date_time>) {
		return U{};
	} else {
		static_assert(sizeof(U) == 0, "Unsupported type for toml::value scalar");
	}
}

template <typename T>
using to_toml_scalar_t = decltype(map_to_toml_scalar_type<T>());

template <typename T>
auto make_toml_value(T&& val) {
	using ScalarT = to_toml_scalar_t<T>;
	return toml::value<ScalarT>{ScalarT(std::forward<T>(val))};
}

class BLACKTHORN_API Settings {
	using ChangeCallback = std::function<void(
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
		return data[section][key].value<T>();
	}

	template <SettingData T>
	[[nodiscard]]
	auto getOr(std::string_view section, std::string_view key, const T& fallback) noexcept {
		auto [secIt, secInserted] = data.insert(section, toml::table{});

		if (auto* secTable = secIt->second.as_table()) {
			auto [keyIt, keyInserted] = secTable->insert(key, fallback);
			return keyIt->second.value_or(fallback);
		}

		return fallback;
	}

	template <SettingData T>
	void set(std::string_view section, std::string_view key, const T& value) {
		auto [it, inserted] = data.insert(section, toml::table{});

		if (auto* sec = it->second.as_table()) {
			auto* existing = sec->get(key);

			if (existing && *existing->value<T>() == make_toml_value(value))
				return;

			toml::node* oldValue = data[section][key].node();

			sec->insert_or_assign(key, value);
			dirty = true;

			if (auto* updated = sec->get(key))
				fireCallbacks(section, key, oldValue, *updated);
		}
	}

	template <SettingData T>
	void setDefault(std::string_view section, std::string_view key, const T& value) {
		auto [it, inserted] = data.insert(section, toml::table{});

		if (auto* sec = it->second.as_table()) {
			sec->insert(key, value);
			dirty = true;
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
				if (auto value = newNode.value<T>())
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

				auto oldValue = oldNode->value<T>();
				auto newValue = newNode.value<U>();

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
	std::flat_map<std::string, std::vector<ChangeCallback>> onChangeCallbacks;
	bool dirty = false;
};

} // namespace Blackthorn::Core