#pragma once

#include <SDL3/SDL.h>

#include "Core/Export.h"
#include "Core/Types/Numeric.h"
#include "Graphics/RenderLayers.h"
#include "Graphics/Texture.h"
#include "Math/Color.h"
#include "Graphics/Types.h"
#include "UI/Container.h"

namespace Blackthorn::UI {

/**
 * @brief A Container with an optional visual background.
 *
 * As a Container, Panel can hold any child widgets. Its background is drawn
 * before its children so that it always appears behind them.
 */
class BLACKTHORN_API Panel : public Container {
public:
	enum class RenderMode : U8 {
		/// No texture is set; draws a filled quad using @c color.
		Solid,
		/// A texture is set and @c borderSize == 0; draws a scaled textured
		/// quad, optionally tinted by @c color.
		Texture,
		/// a texture is set and @c borderSize > 0; draws a nine-slice
		/// textured quad, preserving corner sizes at any dimensions, optionally
		/// tinted by @c color.
		NineSlice
	};

protected:
	Graphics::Texture* textureHandle = nullptr;

	Math::Color color = Math::Colors::White;
	float zDepth = Graphics::RenderLayers::UI;

	Graphics::SliceMargins sliceMargins;

	RenderMode currentRenderMode() const;
	SDL_FRect makeDestRect() const;

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
	void setSliceMargins(const Graphics::SliceMargins& sm);
	Graphics::SliceMargins getSliceMargins() const { return sliceMargins; }

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