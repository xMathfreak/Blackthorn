#pragma once

#include <SDL3/SDL.h>

#include "Core/Export.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::Net::Connection {

/**
 * @brief Enforcement stage for a peer's current rate-limit status.
 */
enum class RateLimitStage : U8 {
	/// Peer is within limits. All packets accepted.
	Normal,

	/// Peer has exceeded the soft limit for long enough to enter Stage 1.
	/// Packets are dropped silently. No log output.
	Drop,

	/// Peer has sustained or worsened violations beyond the Stage 2
	/// threshold. Packets are dropped and a rate-limited warning is logged.
	Warn,

	/// Peer has accumulated enough Stage 3 score to warrant disconnection.
	/// The @c ConnectionManager will push a Disconnect event this tick.
	Disconnect,
};

/**
 * @brief Configuration knobs for a single peer's rate limits.
 *
 * Copied into each @c PeerRateLimiter from @c ConnectionConfig defaults,
 * then optionally overridden per-peer after connection.
 */
struct RateLimitConfig {
	/// Maximum packets per second before Stage 1 begins accumulating.
	float maxPacketsPerSec = 128.0f;

	/// Maximum bytes per second before Stage 1 begins accumulating.
	float maxBytesPerSec = 256.0f * 1024.0f; // 256 KB/s

	/// Violation score that triggers transition to Stage 2 (warning).
	float stage2Threshold = 3.0f;

	/// Violation score that triggers transition to Stage 3 (disconnect).
	float stage3Threshold = 10.0f;

	/// Rate at which violation scores decay per second when the peer
	/// is within limits. Applied independently to both scores.
	float decayRate = 1.0f;

	/// Multiplier applied to the Stage 3 score accumulation per tick.
	/// At 1.0, Stage 3 builds at the same rate as Stage 2 for a given
	/// violation magnitude.
	float stage3Multiplier = 1.0f;

	/// Minimum interval between Stage 2 warning log lines, in milliseconds.
	/// Prevents log spam when a peer sits just above the Stage 2 threshold.
	U32 warnIntervalMs = 5000;
};

/**
 * @brief Per-peer rate limiter implementing a three-stage tiered response
 * with score-based escalation and time-based decay.
 *
 * @details Each inbound packet is passed to @c update(), which advances
 * the limiter's internal sliding window and violation scores, then returns
 * the current @c RateLimitStage. The caller decides what to do based on
 * the returned stage - typically drop the packet or disconnect the peer.
 *
 * @par Stage transitions
 *
 * Violation scores accumulate proportionally to how far the peer's rate
 * exceeds the configured soft limit, scaled by elapsed time:
 *
 * @code
 * magnitude = (currentRate / softLimit) - 1.0 // 0 when at limit, 1 at 2×, etc.
 * stage2Score += magnitude * dt
 * stage3Score += magnitude * dt * stage3Multiplier
 * @endcode
 *
 * Both scores decay independently when the peer is within limits:
 *
 * @code
 * score = max(0, score - decayRate * dt)
 * @endcode
 *
 * Stage transitions are one-way within a tick: once @c Disconnect
 * is returned, it stays that way until the limiter is reset.
 *
 * @par Measurement window
 *
 * Packet and byte counts are accumulated over a 1-second sliding window
 * using a coarse bucket approach: two buckets of 500 ms each. The current
 * rate is the sum of both buckets' counts, giving a smooth 1-second
 * estimate without requiring a ring buffer.
 *
 * @par Thread safety
 *
 * Not thread-safe. Must be accessed under the caller's peer lock
 * (@c ConnectionManager::peerMutex).
 */
struct BLACKTHORN_API PeerRateLimiter {
	/// Configuration (copied from ConnectionConfig on peer allocation,
	/// optionally overridden per-peer afterwards).
	RateLimitConfig cfg;

	/// Timestamp at which the current bucket started, in ms.
	U64 bucketStartMs = 0;

	/// Packet count in the current 500ms bucket.
	U32 currentBucketPackets = 0;

	/// Byte count in the current 500ms bucket.
	U32 currentBucketBytes = 0;

	/// Packet count in the previous 500ms bucket.
	U32 prevBucketPackets = 0;

	/// Byte count in the previous 500ms bucket.
	U32 prevBucketBytes = 0;

	/// Accumulated Stage 2 violation score. Triggers Warn when it
	/// exceeds @c cfg.stage2Threshold.
	float stage2Score = 0.0f;

	/// Accumulated Stage 3 violation score. Triggers Disconnect
	/// when it exceeds @c cfg.stage3Threshold.
	float stage3Score = 0.0f;

	/// Current enforcement stage.
	RateLimitStage stage = RateLimitStage::Normal;

	/// Timestamp of the last Stage 2 warning log, in ms. Used to throttle
	/// log output to at most once per @c cfg.warnIntervalMs.
	U64 lastWarnMs = 0;

	/// Timestamp of the first tick in which the current stage was entered,
	/// in ms. Included in Stage 3 diagnostic log output.
	U64 stageEnteredMs = 0;

	/// Peak packet rate observed since the last stage reset, in pkts/s.
	float peakPacketRate = 0.0f;

	/// Peak byte rate observed since the last stage reset, in bytes/s.
	float peakByteRate = 0.0f;

	/**
	 * @brief Constructs a limiter with default @c RateLimitConfig values.
	 */
	PeerRateLimiter() = default;

	/**
	 * @brief Constructs a limiter with the provided configuration.
	 * @param config Rate limit parameters.
	 */
	explicit PeerRateLimiter(const RateLimitConfig& config)
		: cfg(config)
	{}

	/**
	 * @brief Records one inbound packet and advances all internal state.
	 *
	 * Must be called once per received packet, before deciding whether to
	 * accept or drop it.
	 *
	 * @param packetBytes Size of the received packet in bytes.
	 * @return The current @c RateLimitStage after processing this packet.
	 */
	RateLimitStage update(size_t packetBytes) {
		const U64 now = SDL_GetTicks();

		if (bucketStartMs == 0)
			bucketStartMs = now;

		const U64 elapsed = now - bucketStartMs;

		if (elapsed >= 1000) {
			prevBucketPackets = 0;
			prevBucketBytes = 0;
			currentBucketPackets = 0;
			currentBucketBytes = 0;
			bucketStartMs = now;
		} else if (elapsed >= 500) {
			prevBucketPackets = currentBucketPackets;
			prevBucketBytes = currentBucketBytes;
			currentBucketPackets = 0;
			currentBucketBytes = 0;
			bucketStartMs += 500;
		}

		currentBucketPackets += 1;
		currentBucketBytes += static_cast<U32>(packetBytes);

		const float packetRate =
			static_cast<float>(prevBucketPackets + currentBucketPackets);

		const float byteRate =
			static_cast<float>(prevBucketBytes + currentBucketBytes);

		if (packetRate > peakPacketRate)
			peakPacketRate = packetRate;

		if (byteRate > peakByteRate)
			peakByteRate = byteRate;

		const float packetMag = (cfg.maxPacketsPerSec > 0.0f)
			? (packetRate / cfg.maxPacketsPerSec) - 1.0f
			: 0.0f;

		const float byteMag = (cfg.maxBytesPerSec > 0.0f)
			? (byteRate / cfg.maxBytesPerSec) - 1.0f
			: 0.0f;

		const float magnitude = (packetMag > byteMag)
			? packetMag
			: byteMag;


		const float dt = SDL_clamp(
			static_cast<float>(elapsed > 0 ? elapsed : 1) / 1000.0f,
			0.0f,
			0.1f
		);

		if (magnitude > 0.0f) {
			stage2Score += magnitude * dt;
			stage3Score += magnitude * dt * cfg.stage3Multiplier;
		} else {
			stage2Score -= cfg.decayRate * dt;
			stage3Score -= cfg.decayRate * dt;
			if (stage2Score < 0.0f) stage2Score = 0.0f;
			if (stage3Score < 0.0f) stage3Score = 0.0f;
		}

		RateLimitStage newStage = RateLimitStage::Normal;

		if (stage3Score >= cfg.stage3Threshold) {
			newStage = RateLimitStage::Disconnect;
		} else if (stage2Score >= cfg.stage2Threshold) {
			newStage = RateLimitStage::Warn;
		} else if (magnitude > 0.0f) {
			newStage = RateLimitStage::Drop;
		}

		if (newStage != stage) {
			if (newStage > stage)
				stageEnteredMs = now;

			stage = newStage;
		}

		return stage;
	}

	/**
	 * @brief Returns true if a Stage 2 warning log should be emitted now.
	 *
	 * Throttled to at most once per @c cfg.warnIntervalMs to prevent log
	 * spam when a peer sits persistently at Stage 2.
	 */
	bool shouldWarn() {
		const U64 now = SDL_GetTicks();
		if (now - lastWarnMs >= cfg.warnIntervalMs) {
			lastWarnMs = now;
			return true;
		}

		return false;
	}

	/**
	 * @brief Returns the duration in milliseconds that the peer has been
	 * in the current stage.
	 */
	U64 stageDurationMs() const {
		return SDL_GetTicks() - stageEnteredMs;
	}

	/**
	 * @brief Resets all state to @c Normal as if the peer just connected.
	 *
	 * Preserves the @c cfg so per-peer overrides are not lost.
	 */
	void reset() {
		bucketStartMs = 0;
		currentBucketPackets = 0;
		currentBucketBytes = 0;
		prevBucketPackets = 0;
		prevBucketBytes = 0;
		stage2Score = 0.0f;
		stage3Score = 0.0f;
		stage = RateLimitStage::Normal;
		lastWarnMs = 0;
		stageEnteredMs = 0;
		peakPacketRate = 0.0f;
		peakByteRate = 0.0f;
	}
};

} // namespace Blackthorn::Net::Connection