#pragma once

#include <glm/glm.hpp>

#include "Core/Export.h"

namespace Blackthorn::Graphics {

class Renderer;

/**
 * @brief Standalone 2D orthographic camera.
 *
 * Maintains a world-space transform (position, rotation, zoom) and converts
 * it to a view matrix that can be applied to the @c Renderer. The camera is
 * intentionally decoupled from the ECS and from the @c Renderer. It holds
 * no pointer to either and can be used standalone, stored by value inside a
 * scene or player controller, or wrapped by an ECS system.
 *
 * @section coordinate_system Coordinate system
 * The camera works in the same top-left-origin, Y-down space as the
 * renderer's orthographic projection:
 *   - Positive X → right
 *   - Positive Y → down
 *   - Zoom > 1.0 → more of the world is visible
 *   - Zoom < 1.0 → less of the world is visible (zoom in)
 *
 * @section typical_usage Typical usage
 * @code
 * Camera2D camera;
 * camera.setPosition({ 640.0f, 360.0f });
 * camera.setZoom(2.0f); // see twice as much of the world
 *
 * // In your render function:
 * camera.applyToRenderer(renderer);
 * renderer.beginScene();
 * // draw calls ...
 * renderer.endScene();
 * @endcode
 *
 * @section smooth_follow Smooth follow
 * @code
 * // Lerp toward a target each frame:
 * camera.setPosition(glm::mix(camera.getPosition(), target, dt * followSpeed));
 * @endcode
 */
class BLACKTHORN_API Camera2D {
public:
	Camera2D() = default;

	/**
	 * @brief Constructs a camera centered at @p position.
	 *
	 * @param position  Initial world-space position of the camera centre.
	 * @param zoom      Initial zoom factor (default 1.0).
	 * @param rotation  Initial rotation in radians (default 0.0).
	 */
	explicit Camera2D(const glm::vec2& position, float zoom = 1.0f, float rotation = 0.0f);

	/**
	 * @brief Sets the world-space position of the camera centre.
	 *
	 * @param position New position in world units.
	 */
	void setPosition(const glm::vec2& position) noexcept;

	/**
	 * @brief Moves the camera by @p delta world units.
	 */
	void move(const glm::vec2& delta) noexcept;

	/**
	 * @brief Sets the zoom factor.
	 *
	 * Values > 1 zoom out (show more of the world); values < 1 zoom in.
	 * Clamped to [@c minZoom, @c maxZoom] if limits are set.
	 *
	 * @param zoom New zoom factor. Must be > 0.
	 */
	void setZoom(float zoom) noexcept;

	/**
	 * @brief Multiplies the current zoom by @p factor.
	 *
	 * Useful for scroll-wheel zoom: @c camera.zoom(1.0f + scrollDelta * 0.1f).
	 */
	void zoom(float factor) noexcept;

	/**
	 * @brief Sets the camera rotation in radians.
	 *
	 * Rotation is applied around the camera's position. Positive values
	 * rotate the view clockwise (world content appears to rotate counter-
	 * clockwise).
	 *
	 * @param radians Rotation angle in radians.
	 */
	void setRotation(float radians) noexcept;

	/**
	 * @brief Rotates the camera by @p radians.
	 */
	void rotate(float radians) noexcept;

	/**
	 * @brief Sets the minimum and maximum allowed zoom factors.
	 *
	 * Both values must be > 0 and @p min must be ≤ @p max.
	 * Passing 0 for both disables limiting.
	 *
	 * @param min Minimum zoom factor.
	 * @param max Maximum zoom factor.
	 */
	void setZoomLimits(float min, float max) noexcept;

	/** @brief Removes zoom clamping. */
	void clearZoomLimits() noexcept;

	/**
	 * @brief Returns the view matrix for the current camera state.
	 *
	 * The matrix is recomputed on demand when the transform is dirty.
	 * Calling this multiple times without changing the transform is free.
	 */
	[[nodiscard]] const glm::mat4& getViewMatrix() const noexcept;

	/**
	 * @brief Uploads the camera's view matrix to @p renderer via
	 *        @c Renderer::setView().
	 *
	 * @param renderer The renderer to update.
	 */
	void applyToRenderer(Renderer& renderer) const;

	/**
	 * @brief Converts a screen-space pixel position to world-space.
	 *
	 * @param screenPos  Pixel position (origin at top-left of the window).
	 * @param renderSize Logical render size in pixels (@c renderer.getRenderSize()).
	 * @return World-space position.
	 */
	[[nodiscard]] glm::vec2 screenToWorld(const glm::vec2& screenPos, const glm::ivec2& renderSize) const noexcept;

	/**
	 * @brief Converts a world-space position to screen-space pixels.
	 *
	 * @param worldPos   World-space position.
	 * @param renderSize Logical render size in pixels (@c renderer.getRenderSize()).
	 * @return Screen-space pixel position.
	 */
	[[nodiscard]] glm::vec2 worldToScreen(const glm::vec2& worldPos, const glm::ivec2& renderSize) const noexcept;

	// -------------------------------------------------------------------------
	// Accessors
	// -------------------------------------------------------------------------

	[[nodiscard]] glm::vec2 getPosition() const noexcept { return position; }
	[[nodiscard]] float getZoom() const noexcept { return zoomFactor; }
	[[nodiscard]] float getRotation() const noexcept { return rotationRad; }

private:
	glm::vec2 position { 0.0f, 0.0f };
	float zoomFactor { 1.0f };
	float rotationRad { 0.0f };

	float minZoom { 0.0f }; ///< 0 = no lower limit
	float maxZoom { 0.0f }; ///< 0 = no upper limit

	mutable glm::mat4 viewMatrix { 1.0f };
	mutable bool dirty { true };

	/**
	 * @brief Recomputes the view matrix from the current transform.
	 *
	 * Called lazily from @c getViewMatrix() when @c dirty is true.
	 */
	void recompute() const noexcept;

	/**
	 * @brief Clamps @c zoomFactor to [minZoom, maxZoom] if limits are active.
	 */
	void clampZoom() noexcept;
};

} // namespace Blackthorn::Graphics