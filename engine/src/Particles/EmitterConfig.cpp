#include "Particles/EmitterConfig.h"

#include <algorithm>
#include <cmath>

#include "glm/gtc/constants.hpp"

namespace Blackthorn::Particles {

glm::vec2 sample(const Point&, Distribution, Math::Random&) {
	return {0.0f, 0.0f};
}

glm::vec2 sample(const Line& shape, Distribution, Math::Random& rng) {
	const float halfLen = shape.length * 0.5f;
	return {
		rng.range(-halfLen, halfLen),
		0.0f
	};
}

glm::vec2 sample(const Box& shape, Distribution distribution, Math::Random& rng) {
	const glm::vec2 halfSize = shape.size * 0.5f;

	switch (distribution) {
		case Distribution::Random:
			return {
				rng.range(-halfSize.x, halfSize.x),
				rng.range(-halfSize.y, halfSize.y)
			};

		case Distribution::Boundary: {
			const float perimeter = 2.0f * (shape.size.x + shape.size.y);
			const float d = rng.range(0.0f, perimeter);

			if (d < shape.size.x) {
				return {
					-halfSize.x + d,
					-halfSize.y
				};
			}

			if (d < shape.size.x + shape.size.y) {
				return {
					halfSize.x,
					-halfSize.y + (d - shape.size.x)
				};
			}

			if (d < 2.0f * shape.size.x + shape.size.y) {
				return {
					halfSize.x - (d - (shape.size.x + shape.size.y)),
					halfSize.y
				};
			}

			return {
				-halfSize.x,
				halfSize.y - (d - (2.0f * shape.size.x + shape.size.y))
			};
		}
	}

	return {0.0f, 0.0f};
}

glm::vec2 sample(const Circle& shape, Distribution distribution, Math::Random& rng) {
	switch (distribution) {
		case Distribution::Random: {
			const float angle = rng.range(0.0f, glm::two_pi<float>());
			const float radius = std::sqrt(rng.range(0.0f, 1.0f)) * shape.radius;

			return {
				std::cos(angle) * radius,
				std::sin(angle) * radius
			};
		}

		case Distribution::Boundary: {
			const float angle = rng.range(0.0f, glm::two_pi<float>());

			return {
				std::cos(angle) * shape.radius,
				std::sin(angle) * shape.radius
			};
		}
	}

	return {0.0f, 0.0f};
}

EmissionSample EmitterConfig::sampleEmission(Math::Random& rng) const {
	const glm::vec2 localPosition = std::visit(
		[&](const auto& sh) {
			return sample(sh, distribution, rng);
		}, this->shape
	);

	float r = rotation.sample(rng);

	const float c = std::cos(r);
	const float s = std::sin(r);

	const glm::vec2 rotated {
		localPosition.x * c - localPosition.y * s,
		localPosition.x * s + localPosition.y * c
	};

	return {offset + rotated, r};
}

U32 EmitterConfig::resolveCapacity() const {
	if (maxParticles != 0)
		return maxParticles;

	const float steadyState = rate * lifetimeRange.maxVal;
	return static_cast<U32>(std::ceil(std::max(steadyState, 0.0f)));
}

} // namespace Blackthorn::Particles
