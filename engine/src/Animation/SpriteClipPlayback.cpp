#include "Animation/SpriteClipPlayback.h"

#include "Core/Types/Numeric.h"

namespace Blackthorn::Animation {

namespace {

/**
 * @brief Moves currentFrame to the next frame according to the clip's loop mode.
 */
void stepFrame(const SpriteClip& clip, U32& currentFrame, I8& pingPongDir, bool& playing) {
	const U32 last = clip.frameCount() - 1;

	switch (clip.loopMode) {
		case LoopMode::Once: {
			if (currentFrame < last) {
				++currentFrame;
			} else {
				playing = false;
			}

			break;
		}

		case LoopMode::Loop: {
			currentFrame = (currentFrame + 1) % (last + 1);
			break;
		}

		case LoopMode::PingPong: {
			if (last == 0)
				break;

			if (currentFrame == last) {
				pingPongDir = -1;
			} else if (currentFrame == 0) {
				pingPongDir = 1;
			}

			currentFrame = static_cast<U32>(static_cast<I32>(currentFrame) + pingPongDir);
			break;
		}
	}
}

} // namespace

void advanceClip(
	const SpriteClip& clip,
	U32& currentFrame,
	float& elapsed,
	I8& pingPongDir,
	bool& playing,
	float dt
) {
	elapsed += dt;

	U32 guard = 0;
	while (playing && elapsed >= clip.frames[currentFrame].duration && guard < clip.frameCount()) {
		elapsed -= clip.frames[currentFrame].duration;
		stepFrame(clip, currentFrame, pingPongDir, playing);
		++guard;
	}
}

} // namespace Blackthorn::Animation
