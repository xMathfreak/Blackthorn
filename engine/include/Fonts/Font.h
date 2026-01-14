#pragma once

#include <string_view>

#include <glm/glm.hpp>
#include <SDL3/SDL.h>

#include "Fonts/TextAlign.h"

namespace Blackthorn::Fonts {

struct TextMetrics {
	float width;
	float height;
	size_t lineCount;
};

class Font {
public:
	virtual ~Font() = default;

	virtual void draw(std::string_view text, const glm::vec2& position, float scale = 1.0f, float maxWidth = 0.0f, const SDL_FColor& color = {1.0f, 1.0f, 1.0f, 1.0f}, TextAlign alignment = TextAlign::TopLeft) = 0;
	virtual void drawCached(std::string_view text, const glm::vec2& position, float scale = 1.0f, float maxWidth = 0.0f, const SDL_FColor& color = {1.0f, 1.0f, 1.0f, 1.0f}, TextAlign alignment = TextAlign::TopLeft) = 0;
	
	virtual TextMetrics measure(std::string_view text, float scale, float maxWidth) const = 0;
	virtual float getLineHeight() const = 0;
};

} // namespace Blackthorn::Fonts