#pragma once

#include <string>
#include <vector>

#include "Math/Color.h"

namespace Blackthorn::Fonts {

struct TextStyle {
	Math::Color color = Math::Colors::White;
	bool bold = false;
	bool italic = false;
};

struct MarkupResult {
	std::string plainText;
	std::vector<TextStyle> charStyle;
};

MarkupResult parseMarkup(std::string_view text);
TextStyle parseTag(std::string_view tag, const TextStyle& cur);

} // namespace Blackthorn::Fonts