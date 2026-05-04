#pragma once

#include <random>
#include <ranges>
#include <vector>

#include "Core/Export.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::Math {

class BLACKTHORN_API Random {
public:
	explicit Random(U64 seed = std::random_device{}())
		: randomEngine(seed)
	{}

	void setSeed(U64 seed) {
		randomEngine.seed(seed);
	}

	template <typename T>
	requires std::is_arithmetic_v<T>
	[[nodiscard]]
	T range(T min, T max) {
		if (min > max)
			std::swap(min, max);

		if constexpr (std::is_integral_v<T>) {
			std::uniform_int_distribution<T> dist(min, max);
			return dist(randomEngine);
		} else {
			std::uniform_real_distribution<T> dist(min, max);
			return dist(randomEngine);
		}
	}

	template <typename T>
	requires std::is_arithmetic_v<T>
	[[nodiscard]]
	std::vector<T> range(T min, T max, size_t count) {
		if (min > max)
			std::swap(min, max);

		std::vector<T> values;
		values.reserve(count);

		if constexpr (std::is_integral_v<T>) {
			std::uniform_int_distribution<T> dist(min, max);

			for (size_t i = 0; i < count; ++i)
				values.push_back(dist(randomEngine));
		} else {
			std::uniform_real_distribution<T> dist(min, max);

			for (size_t i = 0; i < count; ++i)
				values.push_back(dist(randomEngine));
		}

		return values;
	}

	template <typename T>
	[[nodiscard]]
	T weighted(const std::vector<std::pair<T, float>>& values) {
		if (values.empty())
			throw std::invalid_argument("Weighted random requires non-empty values");

		auto weights = values | std::views::transform([](const auto& p) {
			return p.second;
		});

		std::discrete_distribution<size_t> dist(weights.begin(), weights.end());
		size_t index = dist(randomEngine);

		return values[index].first;
	}

	template <typename T>
	[[nodiscard]]
	std::vector<T> weighted(const std::vector<std::pair<T, float>>& values, size_t count) {
		if (values.empty())
			throw std::invalid_argument("Weighted random requires non-empty values");

		std::vector<T> results;
		results.reserve(count);

		auto weights = values | std::views::transform([](const auto& p) {
			return p.second;
		});

		std::discrete_distribution<size_t> dist(weights.begin(), weights.end());

		for (size_t i = 0; i < count; ++i) {
			size_t index = dist(randomEngine);
			results.push_back(values[index].first);
		}

		return results;
	}

private:
	std::mt19937_64 randomEngine;
};

} // namespace Blackthorn::Math