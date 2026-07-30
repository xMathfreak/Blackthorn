#pragma once

#include "Animation/SpriteClip.h"
#include "Core/Export.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::ECS::Components {

/**
 * @brief Per-entity playback state for a @c Animation::SpriteClip.
 *
 * Requires a @c Sprite component on the same entity - @c Systems::AnimationSystem
 * writes the current frame's rect into @c Sprite::sourceRect, the same way
 * @c Kinematics requires @c Transform.
 *
 * @note Client-side only. Deliberately not registered with
 * @c Serialization::SerializerRegistry - playback state is derived visual
 * state, not simulation state, and is never sent over the network or saved.
 */
struct BLACKTHORN_API SpriteAnimation {
	/// Clip providing frame data. Not owned - must outlive the component.
	const Animation::SpriteClip* clip = nullptr;

	/// Index of the currently displayed frame within clip->frames.
	U32 currentFrame = 0;

	/// Time accumulated within the current frame, in seconds.
	float elapsed = 0.0f;

	/// Playback rate multiplier. 1.0 = normal speed.
	float speed = 1.0f;

	/// Whether the animation is currently advancing.
	bool playing = true;

	/// Internal direction for LoopMode::PingPong (+1 or -1).
	I8 pingPongDir = 1;

	SpriteAnimation() = default;
	explicit SpriteAnimation(const Animation::SpriteClip* c) : clip(c) {}
};

} // namespace Blackthorn::ECS::Components
