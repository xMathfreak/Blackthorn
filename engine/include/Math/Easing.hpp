#pragma once

#include <cmath>
#include <numbers>

#include "Math/Interpolation.h"

namespace Blackthorn::Math::Easing {

[[nodiscard]]
constexpr float linear(float t) {
	return t;
}

[[nodiscard]]
constexpr float quadIn(float t) {
	return t * t;
}

[[nodiscard]]
constexpr float quadOut(float t) {
	return t * (2.0f - t);
}

[[nodiscard]]
constexpr float quadInOut(float t) {
	return t < 0.5f
		? 2.0f * t * t
		: -1.0f + (4.0f - 2.0f * t) * t;
}

[[nodiscard]]
constexpr float cubicIn(float t) {
	return t * t * t;
}

[[nodiscard]]
constexpr float cubicOut(float t) {
	float f = t - 1.0f;
	return f * f * f + 1.0f;
}

[[nodiscard]]
constexpr float cubicInOut(float t) {
	return t < 0.5f
		? 4.0f * t * t * t
		: (t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f;
}

[[nodiscard]]
constexpr float quartIn(float t) {
	return t * t * t * t;
}

[[nodiscard]]
constexpr float quartOut(float t) {
	float f = t - 1.0f;
	return 1.0f - f * f * f * f;
}

[[nodiscard]]
constexpr float quartInOut(float t) {
	float f = t - 1.0f;
	return t < 0.5f
		? 8.0f * t * t * t * t
		: 1.0f - 8.0f * f * f * f * f;
}

[[nodiscard]]
constexpr float quintIn(float t) {
	return t * t * t * t * t;
}

[[nodiscard]]
constexpr float quintOut(float t) {
	float f = t - 1.0f;
	return f * f * f * f * f + 1.0f;
}

[[nodiscard]]
constexpr float quintInOut(float t) {
	float f = t - 1.0f;
	return t < 0.5f
		? 16.0f * t * t * t * t * t
		: 1.0f + 16.0f * f * f * f * f * f;
}

[[nodiscard]]
inline float sineIn(float t) {
	return 1.0f - std::cos(t * std::numbers::pi_v<float> * 0.5f);
}

[[nodiscard]]
inline float sineOut(float t) {
	return std::sin(t * std::numbers::pi_v<float> * 0.5f);
}

[[nodiscard]]
inline float sineInOut(float t) {
	return 0.5f * (1.0f - std::cos(std::numbers::pi_v<float> * t));
}

[[nodiscard]]
inline float expoIn(float t) {
	return t == 0.0f
		? 0.0f
		: std::pow(2.0f, 10.0f * t - 10.0f);
}

[[nodiscard]]
inline float expoOut(float t) {
	return t == 1.0f
		? 1.0f
		: 1.0f - std::pow(2.0f, -10.0f * t);
}

[[nodiscard]]
inline float expoInOut(float t) {
	if (t == 0.0f)
		return 0.0f;

	if (t == 1.0f)
		return 1.0f;

	float powVal = std::pow(2.0f, 20.0f * t - 10.0f);
	return t < 0.5f
		? powVal * 0.5f
		: (2.0f - std::pow(2.0f, -20.0f * t + 10.0f)) * 0.5f;
}

[[nodiscard]]
inline float circIn(float t) {
	t = std::clamp(t, 0.0f, 1.0f);
	return 1.0f - std::sqrt(1.0f - t * t);
}

[[nodiscard]]
inline float circOut(float t) {
	t = std::clamp(t, 0.0f, 1.0f);
	return std::sqrt((2.0f - t) * t);
}

[[nodiscard]]
inline float circInOut(float t) {
	t = std::clamp(t, 0.0f, 1.0f);
	return t < 0.5f
		? 0.5f * (1.0f - std::sqrt(1.0f - 4.0f * t * t))
		: 0.5f * (std::sqrt(-(2.0f * t - 3.0f) * (2.0f * t - 1.0f)) + 1.0f);
}

[[nodiscard]] constexpr float backIn(float t, float overshoot = 1.70158f) {
	return t * t * ((overshoot + 1.0f) * t - overshoot);
}

[[nodiscard]] constexpr float backOut(float t, float overshoot = 1.70158f) {
	float f = t - 1.0f;
	return f * f * ((overshoot + 1.0f) * f + overshoot) + 1.0f;
}

[[nodiscard]] constexpr float backInOut(float t, float overshoot = 1.70158f) {
	float s = overshoot * 1.525f;
	float f = t * 2.0f;

	if (f < 1.0f)
		return 0.5f * (f * f * ((s + 1.0f) * f - s));

	f -= 2.0f;
	return 0.5f * (f * f * ((s + 1.0f) * f + s) + 2.0f);
}

[[nodiscard]] inline float elasticIn(float t, float amplitude = 1.0f, float period = 0.3f) {
	if (t == 0.0f)
		return 0.0f;

	if (t == 1.0f)
		return 1.0f;

	amplitude = std::max(amplitude, 1.0f);
	float s = period / (2.0f * std::numbers::pi_v<float>) * std::asin(1.0f / amplitude);
	float f = t - 1.0f;

	return -(amplitude * std::pow(2.0f, 10.0f * f) *
		std::sin((f - s) * 2.0f * std::numbers::pi_v<float> / period));
}

[[nodiscard]] inline float elasticOut(float t, float amplitude = 1.0f, float period = 0.3f) {
	if (t == 0.0f)
		return 0.0f;

	if (t == 1.0f)
		return 1.0f;

	amplitude = std::max(amplitude, 1.0f);
	float s = period / (2.0f * std::numbers::pi_v<float>) * std::asin(1.0f / amplitude);

	return amplitude * std::pow(2.0f, -10.0f * t) *
		std::sin((t - s) * 2.0f * std::numbers::pi_v<float> / period) + 1.0f;
}

[[nodiscard]] inline float elasticInOut(float t, float amplitude = 1.0f, float period = 0.45f) {
	if (t == 0.0f)
		return 0.0f;

	if (t == 1.0f)
		return 1.0f;

	amplitude = std::max(amplitude, 1.0f);
	float s = period / (2.0f * std::numbers::pi_v<float>) * std::asin(1.0f / amplitude);
	float f = 2.0f * t - 1.0f;

	if (f < 0.0f) {
		return -0.5f * (amplitude * std::pow(2.0f, 10.0f * f) *
			std::sin((f - s) * 2.0f * std::numbers::pi_v<float> / period));
	}

	return amplitude * std::pow(2.0f, -10.0f * f) *
		std::sin((f - s) * 2.0f * std::numbers::pi_v<float> / period) * 0.5f + 1.0f;
}

[[nodiscard]] constexpr float bounceOut(float t) {
	if (t < 1.0f / 2.75f)
		return 7.5625f * t * t;

	if (t < 2.0f / 2.75f) {
		t -= 1.5f / 2.75f;
		return 7.5625f * t * t + 0.75f;
	}

	if (t < 2.5f / 2.75f) {
		t -= 2.25f / 2.75f;
		return 7.5625f * t * t + 0.9375f;
	}

	t -= 2.625f / 2.75f;
	return 7.5625f * t * t + 0.984375f;
}

[[nodiscard]] constexpr float bounceIn(float t) {
	return 1.0f - bounceOut(1.0f - t);
}

[[nodiscard]] constexpr float bounceInOut(float t) {
	return t < 0.5f
		? 0.5f * bounceIn(t * 2.0f)
		: 0.5f * bounceOut(t * 2.0f - 1.0f) + 0.5f;
}

template <typename T>
requires std::is_arithmetic_v<T>
[[nodiscard]]
constexpr T ease(T start, T end, float t, float(*easingFn)(float)) {
	return Math::lerp(start, end, easingFn(t));
}

} // namespace Blackthorn::Math::Easing