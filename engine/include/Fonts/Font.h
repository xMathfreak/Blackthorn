#pragma once

#include <string_view>

#include <glm/glm.hpp>

#include "Fonts/TextTypes.h"
#include "Math/Color.h"

namespace Blackthorn::Fonts {

class Font {
public:
	virtual ~Font() = default;

	virtual void draw(std::string_view text, const glm::vec2& position, float scale = 1.0f, float z = 0.0f, float maxWidth = 0.0f, const Math::Color& color = Math::Colors::White, Text::Alignment alignment = Text::Alignment::Left, bool useMarkup = false) = 0;
	virtual void drawCached(std::string_view text, const glm::vec2& position, float scale = 1.0f, float z = 0.0f, float maxWidth = 0.0f, const Math::Color& color = Math::Colors::White, Text::Alignment alignment = Text::Alignment::Left, bool useMarkup = false) = 0;

	virtual Text::Metrics measure(std::string_view text, float scale, float maxWidth, bool useMarkup = false) = 0;
	virtual float getLineHeight() const = 0;
};

} // namespace Blackthorn::Fonts