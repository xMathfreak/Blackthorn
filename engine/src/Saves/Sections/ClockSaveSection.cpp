#include "Saves/Sections/ClockSaveSection.h"

namespace Blackthorn::Saves::Sections {

void ClockSaveSection::write(SectionWriteContext& ctx) {
	ctx.buffer.writeU64(clock.getCurrentTick());
	ctx.buffer.writeF32(clock.getTickDuration());
}

void ClockSaveSection::read(SectionReadContext& ctx) {
	const U64 tick = ctx.buffer.readU64();
	const float tickDuration = ctx.buffer.readF32();

	clock.setTickRate(tickDuration);
	clock.resetTo(tick);
}

} // namespace Blackthorn::Saves::Sections