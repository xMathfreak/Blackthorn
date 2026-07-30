#pragma once

#include "Animation/SpriteClip.h"
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

			advance(anim, *anim.clip, dt);
			sprite.sourceRect = anim.clip->frames[anim.currentFrame].sourceRect;
		});
	}

private:
	/**
	 * @brief Steps @c anim forward by @c dt, crossing as many frame
	 * boundaries as elapsed time requires.
	 *
	 * The iteration count is capped at the clip's frame count per call as a
	 * guard against a zero/near-zero frame duration spinning forever.
	 */
	static void advance(Components::SpriteAnimation& anim, const Animation::SpriteClip& clip, float dt) {
		anim.elapsed += dt * anim.speed;

		U32 guard = 0;
		while (anim.playing && anim.elapsed >= clip.frames[anim.currentFrame].duration && guard < clip.frameCount()) {
			anim.elapsed -= clip.frames[anim.currentFrame].duration;
			stepFrame(anim, clip);
			++guard;
		}
	}

	/**
	 * @brief Moves @c anim.currentFrame to the next frame according to the
	 * clip's loop mode.
	 */
	static void stepFrame(Components::SpriteAnimation& anim, const Animation::SpriteClip& clip) {
		const U32 last = clip.frameCount() - 1;

		switch (clip.loopMode) {
		case Animation::LoopMode::Once:
			if (anim.currentFrame < last)
				++anim.currentFrame;
			else
				anim.playing = false;
			break;

		case Animation::LoopMode::Loop:
			anim.currentFrame = (anim.currentFrame + 1) % (last + 1);
			break;

		case Animation::LoopMode::PingPong:
			if (last == 0)
				break;

			if (anim.currentFrame == last)
				anim.pingPongDir = -1;
			else if (anim.currentFrame == 0)
				anim.pingPongDir = 1;

			anim.currentFrame = static_cast<U32>(static_cast<I32>(anim.currentFrame) + anim.pingPongDir);
			break;
		}
	}
};

} // namespace Blackthorn::ECS::Systems
