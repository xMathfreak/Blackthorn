#pragma once

#include "Animation/SpriteClip.h"
#include "Animation/SpriteClipPlayback.h"
#include "Core/Types/Numeric.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/SpriteAnimation.h"
#include "ECS/ISystem.h"

namespace Blackthorn::ECS::Systems {

/**
 * @brief Advances @c Components::SpriteAnimation playback and writes the
 * resulting frame rect into @c Components::Sprite::sourceRect.
 *
 * Runs in @c fixedUpdate so frame advancement is tied to simulation ticks
 * rather than render framerate.
 *
 * Frame-stepping itself is implemented once in @c Animation::advanceClip, this
 * system is just the ECS-side glue.
 *
 * Client-side only: only add this system to the client's SystemManager, not
 * the headless server's. @c SpriteAnimation carries no simulation state, so
 * skipping it server-side changes nothing about gameplay behavior.
 */
class AnimationSystem : public ISystem {
public:
	void fixedUpdate(ECS::EntityPool* pool, float dt, Jobs::JobSystem* js, U64 tick) override {
		auto view = pool->view<Components::SpriteAnimation, Components::Sprite>();
		view.eachJobs(js, [dt](Entity, Components::SpriteAnimation& anim, Components::Sprite& sprite) {
			if (!anim.playing || !anim.clip || !anim.clip->isValid())
				return;

			Animation::advanceClip(*anim.clip, anim.currentFrame, anim.elapsed, anim.pingPongDir, anim.playing, dt * anim.speed);
			sprite.sourceRect = anim.clip->frames[anim.currentFrame].sourceRect;
		});
	}
};

} // namespace Blackthorn::ECS::Systems
