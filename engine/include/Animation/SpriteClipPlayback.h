#pragma once

#include "Animation/SpriteClip.h"
#include "Core/Export.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::Animation {

/**
 * @brief Steps a clip's playback frame state forward by @p dt, crossing as
 * many frame boundaries as elapsed time requires.
 *
 * The iteration count is capped at the clip's frame count per call, as a
 * guard against a zero/near-zero frame duration spinning forever.
 *
 * @param clip Clip providing frame durations/count/loop mode. Caller must
 * check `clip.isValid()` first.
 * @param currentFrame In/out: index of the currently displayed frame.
 * @param elapsed In/out: time accumulated within the current frame, seconds.
 * @param pingPongDir In/out: direction for LoopMode::PingPong (+1 or -1).
 * @param playing In/out: cleared to false once LoopMode::Once reaches its
 * last frame.
 * @param dt Frame delta time, in seconds, already multiplied by any
 * playback speed the caller wants applied.
 *
 * @note Calling advanceClip() again while playing is false is a
 * no-op so callers that don't track a "stopped" state (e.g. particles)
 * can pass a fresh `true` each call rather than persisting this flag.
 */
BLACKTHORN_API void advanceClip(
	const SpriteClip& clip,
	U32& currentFrame,
	float& elapsed,
	I8& pingPongDir,
	bool& playing,
	float dt
);

} // namespace Blackthorn::Animation
