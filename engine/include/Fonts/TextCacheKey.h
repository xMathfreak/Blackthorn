#pragma once

#include <string>

#include "Fonts/TextAlign.h"

namespace Blackthorn::Fonts {

struct TextCacheKey {
	std::string text;
	float scale;
	float maxWidth;
	TextAlign alignment;

	size_t hash() const noexcept {
		size_t h = 0;

		h ^= std::hash<std::string>{}(text);
		h ^= std::hash<float>{}(scale) << 1;
		h ^= std::hash<float>{}(maxWidth) << 2;
		h ^= static_cast<size_t>(alignment) << 3;

		return h;
	}

	bool operator==(const TextCacheKey& other) const noexcept {
		return text == other.text
			&& scale == other.scale
			&& maxWidth == other.maxWidth
			&& alignment == other.alignment;
	}
};

} // namespace Blackthorn::Fonts

namespace std {

template <>
struct hash<Blackthorn::Fonts::TextCacheKey> {
	size_t operator()(const Blackthorn::Fonts::TextCacheKey& key) const noexcept {
		return key.hash();
	}
};

} // namespace std