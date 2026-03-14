#pragma once

#include <SDL3/SDL.h>

#include "Core/Export.h"
#include "Graphics/RenderLayers.h"
#include "Graphics/Texture.h"
#include "Math/Color.h"
#include "UI/Container.h"

namespace Blackthorn::UI {

/**
 * @brief A Container with an optional visual background.
 *
 * Panel supports three render modes, selected automatically based on what
 * has been configured:
 *
 *   Solid      — no texture set; draws a filled quad in `color`.
 *   Texture    — texture set, borderSize == 0; draws a scaled textured quad,
 *                optionally tinted by `color`.
 *   NineSlice  — texture set, borderSize  > 0; draws a nine-slice textured
 *                quad so corners stay pixel-perfect at any size, optionally
 *                tinted by `color`.
 *
 * As a Container, Panel can hold any child widgets; the background is drawn
 * before children so it always sits behind them.
 */
class BLACKTHORN_API Panel : public Container {
public:
	enum class RenderMode : Uint8 {
		Solid,
		Texture,
		NineSlice
	};

protected:
	Graphics::Texture* textureHandle = nullptr;

	Math::Color color = Math::Colors::White;
	float zDepth = Graphics::RenderLayers::UI;

	/// Uniform border size used for nine-slice corners (design units).
	/// Setting this to > 0 while a texture is assigned activates NineSlice mode.
	float borderSize = 0.0f;

	RenderMode currentRenderMode() const;
	SDL_FRect makeDestRect() const;
	SDL_FRect makeSliceMargins() const;

public:
	Panel();
	~Panel() override = default;

	Panel(const Panel&) = delete;
	Panel& operator=(const Panel&) = delete;

	Panel(Panel&&) noexcept = default;
	Panel& operator=(Panel&&) noexcept = default;

	void render(Graphics::Renderer& renderer) override;

	/**
	 * @brief Assign a texture.
	 * Automatically switches the panel to Texture or NineSlice mode
	 * (depending on borderSize). Pass an empty/default handle to return to
	 * Solid mode.
	 */
	void setTexture(Graphics::Texture* handle);
	const Graphics::Texture* getTextureHandle() const { return textureHandle; }

	/**
	 * @brief Clear the current texture, returning the panel to Solid mode.
	 */
	void clearTexture();

	/**
	 * @brief Set the uniform corner/edge border size for nine-slice rendering
	 * (design units).
	 *
	 * A value of 0 disables nine-slice and falls back to a plain texture quad.
	 * Has no effect when no texture is set.
	 *
	 * @param size Border size in design-unit pixels (must be >= 0).
	 */
	void setBorderSize(float size);
	float getBorderSize() const { return borderSize; }

	/**
	 * @brief Set the fill colour (Solid mode) or tint (Texture / NineSlice mode).
	 * Defaults to opaque white, which leaves textured panels un-tinted.
	 */
	void setColor(const Math::Color& c);
	const Math::Color& getColor() const { return color; }

	/**
	 * @brief Set the Z-depth used when submitting draw calls.
	 * Higher values render in front of lower values.
	 */
	void setZDepth(float z);
	float getZDepth() const { return zDepth; }

	/** @brief Returns the render mode that will be used on the next render(). */
	RenderMode getRenderMode() const { return currentRenderMode(); }
};

} // namespace Blackthorn::UI