#pragma once

#include <cstddef>

#include <glm/glm.hpp>

#include "Math/Color.h"

namespace Blackthorn::Text {

enum class Alignment {
	Left,
	Center,
	Right
};

struct Metrics {
	float width;
	float height;
	size_t lineCount;
};

/**
 * @brief Shared, non-defaulted-per-override parameter bundle for
 * Font draw calls.
 */
struct DrawParams {
	float scale = 1.0f;
	float z = 0.0f;
	float maxWidth = 0.0f;
	Math::Color color = Math::Colors::White;
	Alignment alignment = Alignment::Left;
	bool useMarkup = false;
};

struct GlyphInstance {
	glm::vec2 position;
	glm::vec2 size;
	glm::vec4 uv;
	Math::Color color;

	float z;
	float scale;
	float rotation;

	glm::vec2 shadowOffset;
	Math::Color shadowColor;
	float shadowBlur;

	// x = strength (0 = no shake), y = angular speed. Per-glyph phase is
	// derived from gl_InstanceID in the shader rather than stored here, so
	// glyphs desync from each other for free with no extra data.
	glm::vec2 shake;
};

} // namespace Blackthorn::Fonts