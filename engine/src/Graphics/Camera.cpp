#include "Blackthorn/Graphics/Camera.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include "Blackthorn/Graphics/Renderer.hpp"

namespace Blackthorn::Graphics {

Camera::Camera(const glm::vec2& pos, float zoom, float rotation)
	: position(pos)
	, zoomFactor(zoom)
	, rotationRad(rotation)
{}

void Camera::setPosition(const glm::vec2& pos) noexcept {
	if (position != pos) {
		position = pos;
		dirty = true;
	}
}

void Camera::move(const glm::vec2& delta) noexcept {
	if (delta.x != 0.0f || delta.y != 0.0f) {
		position += delta;
		dirty = true;
	}
}

void Camera::setZoom(float zoom) noexcept {
	zoomFactor = zoom;
	clampZoom();
	dirty = true;
}

void Camera::zoom(float factor) noexcept {
	zoomFactor *= factor;
	clampZoom();
	dirty = true;
}

void Camera::setRotation(float radians) noexcept {
	if (rotationRad != radians) {
		rotationRad = radians;
		dirty = true;
	}
}

void Camera::rotate(float radians) noexcept {
	if (radians != 0.0f) {
		rotationRad += radians;
		dirty = true;
	}
}

void Camera::setZoomLimits(float min, float max) noexcept {
	minZoom = min;
	maxZoom = max;
	clampZoom();
}

void Camera::clearZoomLimits() noexcept {
	minZoom = 0.0f;
	maxZoom = 0.0f;
}

void Camera::clampZoom() noexcept {
	if (minZoom > 0.0f && zoomFactor < minZoom)
		zoomFactor = minZoom;

	if (maxZoom > 0.0f && zoomFactor > maxZoom)
		zoomFactor = maxZoom;

	if (zoomFactor <= 0.0f)
		zoomFactor = 0.0001f;
}

void Camera::recompute() const noexcept {
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

const glm::mat4& Camera::getViewMatrix() const noexcept {
	if (dirty)
		recompute();

	return viewMatrix;
}

void Camera::applyToRenderer(Renderer& renderer) const {
	renderer.setView(getViewMatrix());
}

glm::vec2 Camera::screenToWorld(const glm::vec2& screenPos, const glm::ivec2& renderSize) const noexcept {
	const glm::vec2 offset =
		screenPos - glm::vec2(renderSize) * 0.5f;

	const float cosR = std::cos(rotationRad);
	const float sinR = std::sin(rotationRad);

	const glm::vec2 scaledOffset = offset / zoomFactor;

	const glm::vec2 rotatedOffset(
		scaledOffset.x * cosR - scaledOffset.y * sinR,
		scaledOffset.x * sinR + scaledOffset.y * cosR
	);

	return position + rotatedOffset;
}

glm::vec2 Camera::worldToScreen(const glm::vec2& worldPos, const glm::ivec2& renderSize) const noexcept {
	const glm::vec2 delta = worldPos - position;

	const float cosR = std::cos(-rotationRad);
	const float sinR = std::sin(-rotationRad);

	const glm::vec2 rotated(
		delta.x * cosR - delta.y * sinR,
		delta.x * sinR + delta.y * cosR
	);

	const glm::vec2 offset = rotated * zoomFactor;

	return offset + glm::vec2(renderSize) * 0.5f;
}

} // namespace Blackthorn::Graphics