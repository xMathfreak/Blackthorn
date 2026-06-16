#pragma once

#include <stdexcept>
#include <string_view>

#include <glm/glm.hpp>
#include <SDL3/SDL.h>

#include "Core/Types/Numeric.h"
#include "Math/Interpolation.h"

namespace Blackthorn {

namespace Math {

// Normalized float RGBA [0, 1]
using Color = glm::vec4;
using Colour = Color;

namespace Colors {
	inline const Color Clear	{ 0.0f };
	inline const Color White	{ 1.0f };
	inline const Color Black	{ 0.0f, 0.0f, 0.0f, 1.0f };
	inline const Color Gray		{ 0.5f, 0.5f, 0.5f, 1.0f };
	inline const Color Red		{ 1.0f, 0.0f, 0.0f, 1.0f };
	inline const Color Green	{ 0.0f, 1.0f, 0.0f, 1.0f };
	inline const Color Blue		{ 0.0f, 0.0f, 1.0f, 1.0f };
	inline const Color Cyan		{ 0.0f, 1.0f, 1.0f, 1.0f };
	inline const Color Magenta	{ 1.0f, 0.0f, 1.0f, 1.0f };
	inline const Color Yellow	{ 1.0f, 1.0f, 0.0f, 1.0f };
	inline const Color Orange	{ 1.0f, 0.65f, 0.0f, 1.0f };
} // namespace Colors

[[nodiscard]]
inline Color fromRGBA8(U8 r, U8 g, U8 b, U8 a = 255) {
	constexpr float inv = 1.0f / 255.0f;
	return Color{ r * inv, g * inv, b * inv, a * inv };
}

[[nodiscard]]
inline Color fromRGBA32(U32 rgba) {
	return fromRGBA8(
		static_cast<U8>((rgba >> 24) & 0xFF),
		static_cast<U8>((rgba >> 16) & 0xFF),
		static_cast<U8>((rgba >>	 8)	& 0xFF),
		static_cast<U8>(rgba 		& 0xFF)
	);
}

[[nodiscard]]
inline U32 toRGBA32(const Color& c) {
	return (
		  static_cast<U32>(c.r * 255.0f + 0.5f) << 24
		| static_cast<U32>(c.g * 255.0f + 0.5f) << 16
		| static_cast<U32>(c.b * 255.0f + 0.5f) << 8
		| static_cast<U32>(c.a * 255.0f + 0.5f)
	);
}

[[nodiscard]]
inline SDL_Color toSDLColor(const Color& c) {
	return SDL_Color{
		static_cast<U8>(c.r * 255),
		static_cast<U8>(c.g * 255),
		static_cast<U8>(c.b * 255),
		static_cast<U8>(c.a * 255)
	};
}

[[nodiscard]]
inline Color fromSDLColor(const SDL_Color& c) {
	return fromRGBA8(c.r, c.g, c.b, c.a);
}

[[nodiscard]]
inline Color fromHex(std::string_view hex) {
	if (!hex.empty() && hex.front() == '#')
		hex.remove_prefix(1);

	const auto hexToNibble = [](char c) constexpr -> U8 {
		if (c >= '0' && c <= '9')
			return static_cast<U8>(c - '0');

		if (c >= 'a' && c <= 'f')
			return static_cast<U8>(c - 'a' + 10);

		if (c >= 'A' && c <= 'F')
			return static_cast<U8>(c - 'A' + 10);

		throw std::invalid_argument("Invalid hex character");
	};

	const auto parseByte = [&](char high, char low) constexpr -> U8 {
		return static_cast<U8>((hexToNibble(high) << 4) | hexToNibble(low));
	};

	U8 r, g, b, a = 255;

	switch (hex.size()) {
		case 3:
			r = static_cast<U8>(hexToNibble(hex[0]) * 17);
			g = static_cast<U8>(hexToNibble(hex[1]) * 17);
			b = static_cast<U8>(hexToNibble(hex[2]) * 17);
			break;

		case 4:
			r = static_cast<U8>(hexToNibble(hex[0]) * 17);
			g = static_cast<U8>(hexToNibble(hex[1]) * 17);
			b = static_cast<U8>(hexToNibble(hex[2]) * 17);
			a = static_cast<U8>(hexToNibble(hex[3]) * 17);
			break;

		case 6:
			r = parseByte(hex[0], hex[1]);
			g = parseByte(hex[2], hex[3]);
			b = parseByte(hex[4], hex[5]);
			break;

		case 8:
			r = parseByte(hex[0], hex[1]);
			g = parseByte(hex[2], hex[3]);
			b = parseByte(hex[4], hex[5]);
			a = parseByte(hex[6], hex[7]);
			break;

		default:
			throw std::invalid_argument("Hex color must be RGB, RGBA, RRGGBB or RRGGBBAA");
	}

	return fromRGBA8(r, g, b, a);
}

[[nodiscard]]
inline Color fromHSV(float h, float s, float v, float a = 1.0f) {
	while (h >= 360.0f)
		h -= 360.0f;

	while (h < 0.0f)
		h += 360.0f;

	const float c = v * s;
	const float hh = h / 60.0f;
	const int i = static_cast<int>(hh);
	const float f = hh - static_cast<float>(i);

	const float x =  c * (1.0f - (f < 0.5f ? (1.0f - 2.0f * f) : (2.0f * f - 1.0f)));
	const float m = v - c;

	float r{}, g{}, b{};

	switch (i) {
		case 0:
			r = c; g = x; b = 0;
			break;
		case 1:
			r = x; g = c; b = 0;
			break;
		case 2:
			r = 0; g = c; b = x;
			break;
		case 3:
			r = 0; g = x; b = c;
			break;
		case 4:
			r = x; g = 0; b = c;
			break;
		default:
			r = c; g = 0; b = x;
			break;
	}

	return Color{ r + m, g + m, b + m, a };
}

[[nodiscard]]
inline glm::vec3 toHSV(const Color& c) {
	const float max = (c.r > c.g ?
		(c.r > c.b ? c.r : c.b)
	:	(c.g > c.b ? c.g : c.b));

	const float min = (c.r < c.g ?
		(c.r < c.b ? c.r : c.b)
	: 	(c.g < c.b ? c.g : c.b));

	float delta = max - min;

	float h = 0.0f;

	if (delta > 0.0f) {
		if (max == c.r) {
			h = 60.0f * ((c.g - c.b) / delta);
			if (h < 0.0f)
				h += 360.0f;

		} else if (max == c.g) {
			h = 60.0f * ((c.b - c.r) / delta + 2.0f);
		} else {
			h = 60.0f * ((c.r - c.g) / delta + 4.0f);
		}
	}

	if (h < 0.0f)
		h += 360.0f;

	float s = max > 0.0f ? delta / max : 0.0f;
	return glm::vec3{ h, s, max };
}

[[nodiscard]]
inline Color lerp(const Color& a, const Color& b, float t) {
	return Color{
		Math::lerp(a.r, b.r, t),
		Math::lerp(a.g, b.g, t),
		Math::lerp(a.b, b.b, t),
		Math::lerp(a.a, b.a, t)
	};
}

[[nodiscard]]
inline Color withAlpha(const Color& c, float alpha) {
	return Color{ c.r, c.g, c.b, alpha };
}

[[nodiscard]]
inline Color darken(const Color& c, float amount) {
	return Color{
		std::max(c.r - amount, 0.0f),
		std::max(c.g - amount, 0.0f),
		std::max(c.b - amount, 0.0f),
		c.a
	};
}

[[nodiscard]]
inline Color lighten(const Color& c, float amount) {
	return Color{
		std::min(c.r + amount, 1.0f),
		std::min(c.g + amount, 1.0f),
		std::min(c.b + amount, 1.0f),
		c.a
	};
}

[[nodiscard]]
inline Color shiftHue(const Color& c, float degrees) {
	glm::vec3 hsv = toHSV(c);
	return fromHSV(hsv.x + degrees, hsv.y, hsv.z, c.a);
}

} // namespace Math

[[nodiscard]]
inline Math::Color operator""_hex(const char* hex, size_t len) {
	return Math::fromHex(std::string_view(hex, len));
}

} // namespace Blackthorn