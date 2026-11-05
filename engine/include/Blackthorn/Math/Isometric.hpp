#pragma once

#include <cmath>

#include <glm/glm.hpp>

namespace Blackthorn::Math {

constexpr glm::vec2 cartesianToIsometric(const glm::vec2& cart) {
	return glm::vec2{
		(cart.x + cart.y),
		(cart.x - cart.y) * 0.5f
	};
}

constexpr glm::vec2 cartesianToIsometric(const glm::vec2& cart, const glm::vec2& tileSize) {
	return glm::vec2{
		(cart.x - cart.y) * (tileSize.x * 0.5f),
		(cart.x + cart.y) * (tileSize.y * 0.5f)
	};
}

constexpr glm::vec2 isometricToCartesian(const glm::vec2& iso) {
	return glm::vec2{
		(iso.x + 2 * iso.y) * 0.5f,
		(iso.x - 2 * iso.y) * 0.5f
	};
}

constexpr glm::vec2 isometricToCartesian(const glm::vec2& iso, const glm::vec2& tileSize) {
	return glm::vec2{
		(iso.x / (tileSize.x * 0.5f) + iso.y / (tileSize.y * 0.5f)) * 0.5f,
		(iso.y / (tileSize.y * 0.5f) - iso.x / (tileSize.x * 0.5f)) * 0.5f
	};
}

constexpr float isoDepth(const glm::vec2& worldPos) {
	return worldPos.x + worldPos.y;
}

constexpr glm::ivec2 worldToTile(const glm::vec2& world, const glm::vec2& tileSize) {
	return glm::ivec2{
		static_cast<int>(std::floor(world.x / tileSize.x)),
		static_cast<int>(std::floor(world.y / tileSize.y))
	};
}

constexpr glm::vec2 tileToWorld(const glm::ivec2& tile, const glm::vec2& tileSize) {
	return glm::vec2{tile} * tileSize;
}

constexpr glm::vec2 worldToScreen(const glm::vec2& world, const glm::vec2& tileSize, const glm::vec2& origin) {
	return cartesianToIsometric(world, tileSize) + origin;
}

constexpr glm::vec2 screenToWorld(const glm::vec2& screen, const glm::vec2& tileSize, const glm::vec2& origin) {
	return isometricToCartesian(screen - origin, tileSize);
}

} //namespace Blackthorn::Math