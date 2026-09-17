#pragma once

#include <bit>

#include "Core/Export.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::Math {

class BLACKTHORN_API Xoshiro256ss {
public:
	using result_type = U64;

	static constexpr U64 min() { return 0; }
	static constexpr U64 max() { return U64_MAX; }

	explicit Xoshiro256ss(U64 n) noexcept {
		seed(n);
	}

	void seed(U64 n) noexcept {
		state_[0] = splitmix64(n);
		state_[1] = splitmix64(n);
		state_[2] = splitmix64(n);
		state_[3] = splitmix64(n);
	}

	U64 operator()() noexcept {
		const U64 result = std::rotl(state_[1] * 5, 7) * 9;
		const U64 t = state_[1] << 17;

		state_[2] ^= state_[0];
		state_[3] ^= state_[1];
		state_[1] ^= state_[2];
		state_[0] ^= state_[3];

		state_[2] ^= t;
		state_[3] = std::rotl(state_[3], 45);

		return result;
	}

private:
	U64 state_[4];

	static U64 splitmix64(U64& state) noexcept {
		U64 z = (state += UINT64_C(0x9E3779B97F4A7C15));
		z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
		z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
		return z ^ (z >> 31);
	}

};

} // namespace Blackthorn::Math