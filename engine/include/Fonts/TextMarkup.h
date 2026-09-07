#pragma once

#include <string>
#include <vector>

#include "Math/Color.h"

namespace Blackthorn::Fonts {

struct TextStyle {
	Math::Color color = Math::Colors::White;
	bool bold = false;
	bool italic = false;

	// 0 = no shake. shakeSpeed has a non-zero default so `[shake=N]` alone
	// produces reasonable motion without also requiring `shakeSpeed=`.
	float shakeStrength = 0.0f;
	float shakeSpeed = 20.0f;
};

struct MarkupResult {
	std::string plainText;
	std::vector<TextStyle> charStyles;
};

MarkupResult parseMarkup(std::string_view text);
TextStyle parseTag(std::string_view tag, const TextStyle& cur);

} // namespace Blackthorn::Fonts