#include "Core/SimClock.h"

#include "Core/Settings.h"
#include "Debug/Logger.h"

namespace Blackthorn::Core {

SimClock::SimClock(float fixedDeltaTime) {
	setTickRate(fixedDeltaTime);
}

void SimClock::tick() {
	++currentTick;
	totalSimulatedTime = static_cast<double>(tickDuration);
}

void SimClock::reset() {
	currentTick = 0;
	totalSimulatedTime = 0.0;
	BT_DEBUG("SimClock: reset tick to 0");
}

void SimClock::setTickRate(float fixedDeltaTime) {
	if (fixedDeltaTime <= 0) {
		BT_WARN("SimClock: invalid tickDuration {:.6f}, ignoring", fixedDeltaTime);
		return;
	}

	tickDuration = fixedDeltaTime;
	tickRate = 1.0f / fixedDeltaTime;
}

void SimClock::save() const {
	auto& s = Settings::instance();
	s.set<U64>("simulation", "tick", currentTick);
	BT_DEBUG("SimClock: saved tick {}", currentTick);
}

void SimClock::load() {
	auto& s = Settings::instance();
	U64 saved = s.get<U64>("simulation", "tick", static_cast<U64>(0));
	currentTick = saved;
	totalSimulatedTime = static_cast<double>(currentTick) * static_cast<double>(tickDuration);

	BT_DEBUG("SimClock: loaded tick {} ({:.3f}s simulated)", currentTick, totalSimulatedTime);
}

} // namespace Blackthorn
