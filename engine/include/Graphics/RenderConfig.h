#pragma once

#include "Core/Export.h"
#include "Core/Types/Numeric.h"
#include "Math/Color.h"

namespace Blackthorn::Graphics {

/**
 * @brief Controls how the internal render resolution relates to the window.
 *
 * The Renderer always draws into an internal FBO at a fixed logical
 * resolution, then presents that FBO to the window in @c endScene(). This
 * enum controls how those two resolutions are kept in sync.
 *
 * @section FollowWindow
 * The FBO is resized to match the window pixel dimensions whenever the window
 * is resized. The internal and window resolutions are always identical.
 * This is the default and matches the original engine behavior exactly.
 *
 * @section Fixed
 * The FBO is created once at @c renderWidth * @c renderHeight and never
 * resized. On presentation the FBO is stretched to fill the entire window
 * via @c glBlitFramebuffer (no post-process shader). Aspect ratio is
 * not preserved. use @c PixelPerfect for that.
 *
 * @section PixelPerfect
 * The FBO is created once at @c renderWidth * @c renderHeight. On
 * presentation it is scaled by the largest integer factor that fits
 * inside the current window dimensions, then centred. The surrounding
 * border is cleared to @c RenderConfig::letterboxColor before the blit.
 *
 * This mode is designed for pixel-art games where sub-pixel scaling
 * introduces unwanted blurring: at integer scale factors every logical
 * pixel maps to an identical number of physical pixels.
 */
enum class RenderResolutionMode {
	/// Resize the FBO to match the window on every window resize event.
	FollowWindow,

	/// Lock th FBO to renderWidth & renderHeight; stretch to fill window.
	Fixed,

	/// Lock the FBO to renderWidth * renderHeight; integer scaled to fit
	/// window and letterbox the remaining border.
	PixelPerfect,
};

struct BLACKTHORN_API RenderConfig {
	int openglMajor = 3;
	int openglMinor = 3;
	int depthBits = 16;
	int stencilBits = 0;

	/// Maximum number of quads per batch.
	/// Drives MAX_VERTICES `(maxQuads * 4)` and
	/// MAX_INDICES `(maxQuads * 6)` inside the Renderer.
	U32 maxQuads = 4096;

	static constexpr U32 maxTextureSlots = 16;

	/// Controls how the internal render resolution tracks the window size.
	RenderResolutionMode resolutionMode = RenderResolutionMode::FollowWindow;

	/// Logical render width in pixels.
	/// Only used when resolutionMode is Fixed or PixelPerfect.
	/// Ignored (and replaced by the window size) in FollowWindow mode.
	int renderWidth = 320;

	/// Logical render height in pixels.
	/// Only used when resolutionMode is Fixed or PixelPerfect.
	int renderHeight = 180;

	/// Color used to clear the border area when letterboxing.
	/// Only visible in PixelPerfect mode when the scaled FBO does not fill
	/// the entire window. Defaults to opaque black.
	Math::Color letterboxColor = Math::Colors::Black;
};

} // namespace Blackthorn::Graphics