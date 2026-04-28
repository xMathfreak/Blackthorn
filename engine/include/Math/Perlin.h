#pragma once

#include <algorithm>
#include <array>
#include <numeric>
#include <random>

#include <glm/glm.hpp>

#include "Core/Export.h"
#include "Core/Types/Types.h"

namespace Blackthorn::Math::Noise {

class BLACKTHORN_API Perlin {
public:
	explicit Perlin(U64 seed = std::random_device{}()) {
		initPermutations(seed);
	}

	void setSeed(U64 seed) {
		initPermutations(seed);
	}

	[[nodiscard]] float sample(float x) const {
		int X = fastFloor(x);

		x -= X;

		X &= 255;

		float u = fade(x);

		int a = perm[X];
		int b = perm[X + 1];

		return lerp(
			grad(a, x, 0.0f, 0.0f),
			grad(b, x - 1.0f, 0.0f, 0.0f),
			u
		);
	}

	[[nodiscard]] float sample(const glm::vec2& v) const {
		int X = fastFloor(v.x);
		int Y = fastFloor(v.y);

		float x = v.x - X;
		float y = v.y - Y;

		X &= 255;
		Y &= 255;

		float u = fade(x);
		float w = fade(y);

		int aa = perm[perm[X] + Y];
		int ab = perm[perm[X] + (Y + 1)];
		int ba = perm[perm[X + 1] + Y];
		int bb = perm[perm[X + 1] + (Y + 1)];

		return lerp(
			lerp(grad(aa, x, y, 0.0f),
			grad(ba, x - 1.0f, y, 0.0f), u),
			lerp(grad(ab, x, y - 1.0f, 0.0f),
			grad(bb, x - 1.0f, y - 1.0f, 0.0f), u),
			w
		);
	}

	[[nodiscard]] float sample(const glm::vec3& v) const {
		int X = fastFloor(v.x);
		int Y = fastFloor(v.y);
		int Z = fastFloor(v.z);

		float x = v.x - X;
		float y = v.y - Y;
		float z = v.z - Z;

		X &= 255;
		Y &= 255;
		Z &= 255;

		float u = fade(x);
		float w = fade(y);
		float t = fade(z);

		int aa = perm[perm[perm[X] + Y] + Z];
		int ab = perm[perm[perm[X] + Y] + (Z + 1)];
		int ba = perm[perm[perm[X] + (Y + 1)] + Z];
		int bb = perm[perm[perm[X] + (Y + 1)] + (Z + 1)];

		int ca = perm[perm[perm[X + 1] + Y] + Z];
		int cb = perm[perm[perm[X + 1] + Y] + (Z + 1)];
		int da = perm[perm[perm[X + 1] + (Y + 1)] + Z];
		int db = perm[perm[perm[X + 1] + (Y + 1)] + (Z + 1)];

		return lerp(
			lerp(
				lerp(grad(aa, x, y, z),
					 grad(ca, x - 1.0f, y, z), u),
				lerp(grad(ba, x, y - 1.0f, z),
					 grad(da, x - 1.0f, y - 1.0f, z), u),
				w
			),
			lerp(
				lerp(grad(ab, x, y, z - 1.0f),
					 grad(cb, x - 1.0f, y, z - 1.0f), u),
				lerp(grad(bb, x, y - 1.0f, z - 1.0f),
					 grad(db, x - 1.0f, y - 1.0f, z - 1.0f), u),
				w
			),
			t
		);
	}

	template <typename T>
	requires std::same_as<T, glm::vec2> || std::same_as<T, glm::vec3>
	[[nodiscard]]
	float fbm(const T& v, int octaves = 4, float lacunarity = 2.0f, float gain = 0.5f) const {
		if (octaves <= 0)
			return 0.0f;

		float value = 0.0f;
		float amplitude = 1.0f;
		float frequency = 1.0f;
		float max = 0.0f;

		for (int i = 0; i < octaves; ++i) {
			value += sample(v * frequency) * amplitude;
			max += amplitude;
			amplitude *= gain;
			frequency *= lacunarity;
		}

		return value / max;
	}

private:
	std::array<int, 512> perm;

	static float fade(float t) {
		return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
	}

	static float lerp(float a, float b, float t) {
		return a + t * (b - a);
	}

	static int fastFloor(float x) {
		int xi = static_cast<int>(x);
		return xi - (x < xi);
	}

	static float grad(int hash, float x, float y, float z) {
		int h = hash & 15;
		float u = h < 8 ? x : y;
		float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
		return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
	}

	void initPermutations(U64 seed) {
		std::iota(perm.begin(), perm.begin() + 256, 0);
		std::shuffle(perm.begin(), perm.begin() + 256, std::mt19937_64(seed));

		for (int i = 0; i < 256; ++i)
			perm[256 + i] = perm[i];
	}
};

} // namespace Blackthorn::Math::Noise