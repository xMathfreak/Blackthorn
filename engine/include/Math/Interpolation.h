#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

namespace Blackthorn::Math {

template <typename T>
requires std::is_arithmetic_v<T>
constexpr T lerp(T start, T end, T t) {
	return start + (end - start) * t;
}

template <typename T>
requires std::is_arithmetic_v<T>
constexpr T inverseLerp(T start, T end, T value) {
	return (value - start) / (end - start);
}

template <typename T>
requires std::is_arithmetic_v<T>
constexpr T remap(T value, T inMin, T inMax, T outMin, T outMax) {
	return lerp(outMin, outMax, inverseLerp(inMin, inMax, value));
}

template <typename T>
requires std::is_arithmetic_v<T>
constexpr T lerpClamp(T start, T end, T t) {
	return lerp(start, end, std::clamp(t, T{0}, T{1}));
}

template <typename T>
requires std::is_arithmetic_v<T>
constexpr T smoothstep(T start, T end, T t) {
	t = std::clamp((t - start) / (end - start), T{0}, T{1});
	return t * t * (T{3} - T{2} * t);
}

template <typename T>
requires std::is_arithmetic_v<T>
constexpr T smootherstep(T start, T end, T t) {
	t = std::clamp((t - start) / (end - start), T{0}, T{1});
	return t * t * t * (t * (t * T{6} - T{15}) + T{10});
}

template <typename T>
requires std::is_arithmetic_v<T>
T expDecay(T start, T end, T decay, float deltaTime) {
	return end + (start - end) * std::exp(-decay * deltaTime);
}

template <typename T>
requires std::is_arithmetic_v<T>
constexpr T hermite(T start, T startTangent, T end, T endTangent, T t) {
	T t2 = t * t;
	T t3 = t2 * t;
	return (T{2} * t3 - T{3} * t2 + T{1}) * start
		 + (t3 - T{2} * t2 + t) * startTangent
		 + (T{-2} * t3 + T{3} * t2) * end
		 + (t3 - t2) * endTangent;
}

} //namespace Blackthorn::Math