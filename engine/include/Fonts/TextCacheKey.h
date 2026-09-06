#pragma once

#include <string>

#include "Fonts/TextTypes.h"

namespace Blackthorn::Fonts {

struct TextCacheKey {
	std::string text;
	float scale;
	float maxWidth;
	Text::Alignment alignment;
	bool markup = false;

	size_t hash() const noexcept {
		size_t h = std::hash<std::string>{}(text);
		h ^= std::hash<float>{}(scale)    * 2654435761ULL;
		h ^= std::hash<float>{}(maxWidth) * 2246822519ULL;
		h ^= static_cast<size_t>(alignment) * 3266489917ULL;
		h ^= static_cast<size_t>(markup) * 668265263ULL;

		return h;
	}

	bool operator==(const TextCacheKey& other) const noexcept {
		return text == other.text
			&& scale == other.scale
			&& maxWidth == other.maxWidth
			&& alignment == other.alignment
			&& markup == other.markup;
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