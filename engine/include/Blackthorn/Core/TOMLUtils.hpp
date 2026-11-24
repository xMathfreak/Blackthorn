#pragma once

#include <charconv>
#include <concepts>
#include <limits>
#include <optional>
#include <string>
#include <system_error>

#include <toml++/toml.hpp>

#include "Blackthorn/Types/Numeric.hpp"

namespace Blackthorn::Core {

template <typename T>
concept UnsignedInteger =
	std::is_integral_v<std::remove_cvref_t<T>> &&
	std::is_unsigned_v<std::remove_cvref_t<T>> &&
	!std::is_same_v<std::remove_cvref_t<T>, bool>;

template <typename T>
std::optional<T> fromTOMLValue(const toml::node& node) {
	using U = std::remove_cvref_t<T>;

	if constexpr (UnsignedInteger<U>) {
		if (auto value = node.value<I64>()) {
			if (*value < 0)
				return std::nullopt;

			if constexpr (
				std::numeric_limits<U>::max() <
				U64_MAX
			) {
				if (static_cast<U64>(*value) >
					std::numeric_limits<U>::max()) {
					return std::nullopt;
				}
			}

			return static_cast<U>(*value);
		}

		// Large unsigned integers are stored as strings.
		if (auto str = node.value<std::string>()) {
			U result{};

			const auto [ptr, ec] = std::from_chars(
				str->data(),
				str->data() + str->size(),
				result
			);

			if (ec == std::errc{} &&
				ptr == str->data() + str->size()) {
				return result;
			}
		}

		return std::nullopt;
	} else {
		return node.value<T>();
	}
}

template <typename T>
void insertDefaultTOMLValue(
	toml::table& table,
	std::string_view key,
	const T& value
) {
	using U = std::decay_t<T>;

	if constexpr (UnsignedInteger<U>) {
		if (value <= static_cast<U>(I64_MAX)) {
			table.insert(key, static_cast<I64>(value));
		} else {
			table.insert(key, std::to_string(value));
		}
	} else {
		table.insert(key, value);
	}
}

template <typename T>
void insertTOMLValue(
	toml::table& table,
	std::string_view key,
	const T& value
) {
	using U = std::decay_t<T>;

	if constexpr (UnsignedInteger<U>) {
		if (value <= static_cast<U>(I64_MAX)) {
			table.insert_or_assign(
				key,
				static_cast<I64>(value)
			);
		} else {
			table.insert_or_assign(
				key,
				std::to_string(value)
			);
		}
	} else {
		table.insert_or_assign(key, value);
	}
}

inline std::unique_ptr<toml::node> cloneNode(const toml::node* node) {
	if (!node)
		return nullptr;

	return node->visit([](const auto& concrete) -> std::unique_ptr<toml::node> {
		using ConcreteType = std::remove_cv_t<std::remove_reference_t<decltype(concrete)>>;
		return std::make_unique<ConcreteType>(concrete);
	});
}

} // namespace Blackthorn::Core