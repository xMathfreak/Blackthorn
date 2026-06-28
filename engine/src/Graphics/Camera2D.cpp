#include "Graphics/Camera2D.h"

#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>

#include "Graphics/Renderer.h"

namespace Blackthorn::Graphics {

Camera2D::Camera2D(const glm::vec2& pos, float zoom, float rotation)
	: position(pos)
	, zoomFactor(zoom)
	, rotationRad(rotation)
	, dirty(true)
{}

void Camera2D::setPosition(const glm::vec2& pos) noexcept {
	if (position != pos) {
		position = pos;
		dirty = true;
	}
}

void Camera2D::move(const glm::vec2& delta) noexcept {
	if (delta.x != 0.0f || delta.y != 0.0f) {
		position += delta;
		dirty = true;
	}
}

void Camera2D::setZoom(float zoom) noexcept {
	zoomFactor = zoom;
	clampZoom();
	dirty = true;
}

void Camera2D::zoom(float factor) noexcept {
	zoomFactor *= factor;
	clampZoom();
	dirty = true;
}

void Camera2D::setRotation(float radians) noexcept {
	if (rotationRad != radians) {
		rotationRad = radians;
		dirty = true;
	}
}

void Camera2D::rotate(float radians) noexcept {
	if (radians != 0.0f) {
		rotationRad += radians;
		dirty = true;
	}
}

void Camera2D::setZoomLimits(float min, float max) noexcept {
	minZoom = min;
	maxZoom = max;
	clampZoom();
}

void Camera2D::clearZoomLimits() noexcept {
	minZoom = 0.0f;
	maxZoom = 0.0f;
}

void Camera2D::clampZoom() noexcept {
	if (minZoom > 0.0f && zoomFactor < minZoom)
		zoomFactor = minZoom;

	if (maxZoom > 0.0f && zoomFactor > maxZoom)
		zoomFactor = maxZoom;

	if (zoomFactor <= 0.0f)
		zoomFactor = 0.0001f;
}

void Camera2D::recompute() const noexcept {
	viewMatrix = glm::mat4(1.0f);
	viewMatrix = glm::translate(viewMatrix, glm::vec3(-position, 0.0f));

	if (rotationRad != 0.0f)
		viewMatrix = glm::rotate(viewMatrix, -rotationRad, glm::vec3(0.0f, 0.0f, 1.0f));

	if (zoomFactor != 1.0f) {
		const float invZoom = 1.0f / zoomFactor;
		viewMatrix = glm::scale(viewMatrix, glm::vec3(invZoom, invZoom, 1.0f));
	}

	dirty = false;
}

const glm::mat4& Camera2D::getViewMatrix() const noexcept {
	if (dirty)
		recompute();

	return viewMatrix;
}

void Camera2D::applyToRenderer(Renderer& renderer) const {
	renderer.setView(getViewMatrix());
}

glm::vec2 Camera2D::screenToWorld(const glm::vec2& screenPos, const glm::ivec2& renderSize) const noexcept {
	const float ndcX = (screenPos.x / static_cast<float>(renderSize.x)) * 2.0f - 1.0f;
	const float ndcY = -(screenPos.y / static_cast<float>(renderSize.y)) * 2.0f + 1.0f;

	const float cosR = std::cos(rotationRad);
	const float sinR = std::sin(rotationRad);

	const float halfW = static_cast<float>(renderSize.x) * 0.5f;
	const float halfH = static_cast<float>(renderSize.y) * 0.5f;

	const float vx = ndcX * halfW * zoomFactor;
	const float vy = -ndcY * halfH * zoomFactor;

	const float worldOffsetX = vx * cosR - vy * sinR;
	const float worldOffsetY = vx * sinR + vy * cosR;

	return position + glm::vec2(worldOffsetX, worldOffsetY);
}

glm::vec2 Camera2D::worldToScreen(const glm::vec2& worldPos, const glm::ivec2& renderSize) const noexcept {
	const glm::vec2 delta = worldPos - position;

	const float cosR = std::cos(-rotationRad);
	const float sinR = std::sin(-rotationRad);

	const float rx = delta.x * cosR - delta.y * sinR;
	const float ry = delta.x * sinR + delta.y * cosR;

	const float vx = rx / zoomFactor;
	const float vy = ry / zoomFactor;

	const float halfW = static_cast<float>(renderSize.x) * 0.5f;
	const float halfH = static_cast<float>(renderSize.y) * 0.5f;

	return {
		(vx / halfW + 1.0f) * 0.5f * static_cast<float>(renderSize.x),
		(1.0f - vy / halfH) * 0.5f * static_cast<float>(renderSize.y)
	};
}

} // namespace Blackthorn::Graphics