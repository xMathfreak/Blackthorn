#pragma once

#include "Core/Export.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::Core {

/**
 * @brief Authoritative simulation clock for the engine.
 *
 * Tracks the current tick, tick rate, and tick duration. Owned by
 * `Engine` and exposed read-only through `ISceneContext` so scenes and
 * systems can observe the current tick without coupling to `Engine`.
 *
 * The tick counter is persisted to the INI settings file under
 * `[simulation] tick` so sessions can resume from their last known tick.
 * Call `save()` at shutdown and `load()` after `Settings::loadFromFile()`.
 *
 * @note A "tick" corresponds to one `fixedUpdate` execution. Rendering
 * and variable-rate updates do not advance the tick counter.
 *
 * @note This class is not thread-safe. All mutation must occur on the
 * main thread inside the fixed-update loop.
 */
class BLACKTHORN_API SimClock {
public:
	/**
	 * @brief Constructs a SimClock with the given fixed timestep.
	 * @param fixedDeltaTime Seconds per tick (e.g. 1.0f / 60.0f).
	 */
	explicit SimClock(float fixedDeltaTime);

	/**
	 * @brief Advance the clock by one tick.
	 *
	 * Increments `currentTick` and updates `totalSimulatedTime`.
	 * Called once per `fixedUpdate` execution in `Engine::run()`.
	 */
	void tick();

	/**
	 * @brief Reset the clock to tick zero.
	 *
	 * Resets `currentTick` and `totalSimulatedTime` to zero.
	 * Does not change `tickRate` or `tickDuration`.
	 */
	void reset();

	/**
	 * @brief Change the tick rate at runtime.
	 *
	 * Updates `tickRate` and recomputes `tickDuration`.
	 * Does not reset the tick counter.
	 *
	 * @param fixedDeltaTime New seconds-per-tick value. Must be > 0.
	 */
	void setTickRate(float fixedDeltaTime);

	/**
	 * @brief Reset the clock to a specific tick.
	 *
	 * Sets `currentTick` to the given value and recomputes
	 * `totalSimulatedTime` accordingly.
	 *
	 * @param tick The tick to reset to.
	 */
	void resetTo(U64 tick);

	/**
	 * @brief Persist the current tick to the Settings INI store.
	 *
	 * Writes `currentTick` to `[simulation] tick`.
	 * Call this at engine shutdown or at scene transition checkpoints.
	 */
	void save() const;

	/**
	 * @brief Restore the tick counter from the Settings INI store.
	 *
	 * Reads `[simulation] tick` and restores `currentTick` and
	 * `totalSimulatedTime`. Call this after `Settings::loadFromFile()`.
	 */
	void load();

	/** @brief Returns the number of ticks elapsed since the clock epoch. */
	U64 getCurrentTick() const { return currentTick; }

	/** @brief Returns the number of seconds each tick represents. */
	float getTickDuration() const { return tickDuration; }

	/** @brief Returns the number of ticks per second (1.0f / tickDuration). */
	float getTickRate() const { return tickRate; }

	/**
	 * @brief Returns the total simulated time in seconds.
	 *
	 * Equivalent to `currentTick * tickDuration` but stored explicitly
	 * to avoid floating-point accumulation error over long sessions.
	 */
	double getTotalSimulatedTime() const { return totalSimulatedTime; }

private:
	U64 currentTick = 0;

	float tickDuration = 1.0f / 60.0f;
	float tickRate = 60.0f;

	double totalSimulatedTime = 0.0;
};

} // namespace Blackthorn::Core