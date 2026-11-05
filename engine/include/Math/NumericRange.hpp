#pragma once

#include "Math/Random.h"

namespace Blackthorn::Math {

template <typename T>
requires std::is_arithmetic_v<T>
struct NumericRange {
	T minVal{};
	T maxVal{};

	NumericRange() = default;

	constexpr NumericRange(T val)
		: minVal(val), maxVal(val)
	{}

	constexpr NumericRange(T min, T max)
		: minVal(min), maxVal(max)
	{}

	[[nodiscard]]
	constexpr T sample(Random& rng) const {
		return (minVal == maxVal)
			? minVal
			: rng.range(minVal, maxVal);
	}
};

using IntRange = NumericRange<int>;
using FloatRange = NumericRange<float>;

};