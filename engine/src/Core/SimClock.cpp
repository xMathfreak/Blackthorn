// engine/src/Core/SimClock.cpp
#include "Core/SimClock.h"

#include "Core/Settings.h"
#include "Debug/Logger.h"

namespace Blackthorn::Core {

SimClock::SimClock(float fixedDeltaTime, U64 initial)
	: currentTick(initial)
	, initialTick(initial)
{
	setTickRate(fixedDeltaTime);

	totalSimulatedTime =
		static_cast<double>(currentTick) * static_cast<double>(tickDuration);
}

void SimClock::tick() {
	++currentTick;
	totalSimulatedTime += static_cast<double>(tickDuration);
}

void SimClock::reset() {
	currentTick = initialTick;
	totalSimulatedTime =
		static_cast<double>(initialTick) * static_cast<double>(tickDuration);

	BT_DEBUG("SimClock: reset to initialTick {}", initialTick);
}

void SimClock::setTickRate(float fixedDeltaTime) {
	if (fixedDeltaTime <= 0.0f) {
		BT_WARN("SimClock: invalid tickDuration {:.6f}, ignoring", fixedDeltaTime);
		return;
	}

	tickDuration = fixedDeltaTime;
	tickRate = 1.0f / fixedDeltaTime;
}

void SimClock::resetTo(U64 tick) {
	currentTick = tick;
	totalSimulatedTime =
		static_cast<double>(tick) * static_cast<double>(tickDuration);

	BT_DEBUG(
		"SimClock: reset to tick {} ({:.3f}s simulated)",
		currentTick, totalSimulatedTime
	);
}

void SimClock::save() const {
	auto& s = Settings::instance();
	s.set<U64>("simulation", "tick", currentTick);
	BT_DEBUG("SimClock: saved tick {} to INI", currentTick);
}

void SimClock::load() {
	auto& s = Settings::instance();

	const U64 saved = s.get<U64>(
		"simulation", "tick",
		static_cast<U64>(initialTick)
	);

	resetTo(saved);
	BT_DEBUG("SimClock: loaded tick {} from INI", currentTick);
}

} // namespace Blackthorn::Core