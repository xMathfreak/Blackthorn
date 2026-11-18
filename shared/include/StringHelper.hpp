#pragma once

#include <algorithm>
#include <cctype>
#include <ranges>
#include <string>
#include <string_view>

namespace Blackthorn::String {

namespace Detail {

constexpr std::string_view whitespace = " \t\n\r\f\v";

}

inline std::string toLower(std::string_view str) {
	return str | std::views::transform([](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	}) | std::ranges::to<std::string>();
}

inline void toLowerInPlace(std::string& str) {
	std::ranges::transform(str, str.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
}

inline std::string toUpper(std::string_view str) {
	return str | std::views::transform([](unsigned char c) {
		return static_cast<char>(std::toupper(c));
	}) | std::ranges::to<std::string>();
}

inline void toUpperInPlace(std::string& str) {
	std::ranges::transform(str, str.begin(), [](unsigned char c) {
		return static_cast<char>(std::toupper(c));
	});
}

constexpr std::string_view trim(std::string_view str) noexcept {
	const auto first = str.find_first_not_of(Detail::whitespace);
	if (first == std::string_view::npos)
		return {};

	const auto last = str.find_last_not_of(Detail::whitespace);
	return str.substr(first, last - first + 1);
}

constexpr void trimInPlace(std::string& str) {
	const auto last = str.find_last_not_of(Detail::whitespace);
	if (last == std::string::npos) {
		str.clear();
		return;
	}

	str.erase(last + 1);

	const auto first = str.find_first_not_of(Detail::whitespace);
	if (first != std::string::npos)
		str.erase(0, first);
}

} // namespace Blackthorn