#pragma once

#include <vector>

#include <SDL3/SDL.h>

#include "Core/Export.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::Animation {

/**
 * @brief Controls how a @c SpriteClip behaves once it reaches its last frame.
 */
enum class LoopMode : U8 {
	Once,     ///< Plays once and stops on the final frame.
	Loop,     ///< Restarts from frame 0 after the final frame.
	PingPong, ///< Reverses direction at each end instead of restarting.
};

/**
 * @brief A single frame within a @c SpriteClip.
 */
struct BLACKTHORN_API Frame {
	/// Source rectangle within the sprite's texture, in pixels.
	SDL_FRect sourceRect{0, 0, 0, 0};

	/// How long this frame is displayed, in seconds.
	float duration = 0.1f;
};

/**
 * @brief Shared, reusable frame-animation data for sprites.
 *
 * A @c SpriteClip only describes *which sub-rects of a texture to show and
 * for how long* - it does not own a texture. The texture stays on the
 * entity's @c Sprite component, exactly as it does for non-animated sprites.
 * Many entities can reference the same @c SpriteClip, the same way they
 * already share a single @c Texture asset.
 *
 * Loaded via @c Assets::Loaders::SpriteClipLoader and owned by
 * @c Assets::AssetManager.
 */
class BLACKTHORN_API SpriteClip {
public:
	std::vector<Frame> frames;
	LoopMode loopMode = LoopMode::Loop;

	/**
	 * @brief Returns the number of frames in this clip.
	 */
	U32 frameCount() const noexcept {
		return static_cast<U32>(frames.size());
	}

	/**
	 * @brief Returns true if the clip has at least one frame.
	 */
	bool isValid() const noexcept {
		return !frames.empty();
	}
};

} // namespace Blackthorn::Animation
