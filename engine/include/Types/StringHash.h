#pragma once

#include <string>
#include <string_view>

struct StringHash {
	using is_transparent = void;

	size_t operator()(std::string_view sv) const noexcept {
		return std::hash<std::string_view>{}(sv);
	}

	size_t operator()(const std::string& s) const noexcept {
		return std::hash<std::string_view>{}(s);
	}
};