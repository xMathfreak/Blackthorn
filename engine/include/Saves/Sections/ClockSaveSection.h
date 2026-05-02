#pragma once

#include <string_view>

#include "Core/Export.h"
#include "Core/SimClock.h"
#include "Saves/SaveHash.h"
#include "Saves/Sections/ISaveSection.h"

namespace Blackthorn::Saves::Sections {

/**
 * @brief Built-in save section for simulation clock state (@c bt.clock).
 *
 * Persists the current tick counter and tick duration so a loaded session
 * resumes from exactly where it left off. This supersedes the legacy
 * @c SimClock::save() / @c SimClock::load() path that wrote to the INI
 * settings file.
 *
 * @par Wire layout (payload)
 * @code
 * [uint64 currentTick]
 * [float  tickDuration]   # stored as IEEE 754 single precision
 * @endcode
 *
 * @par Version history
 * - Version 1: Initial layout. currentTick + tickDuration.
 */
class BLACKTHORN_API ClockSaveSection final : public ISaveSection {
public:
	static constexpr std::string_view SECTION_NAME = "bt.clock";
	static constexpr U32 CURRENT_VERSION = 1;

	/**
	 * @brief Constructs a clock section backed by @p clock.
	 * The reference must outlive this section instance.
	 */
	explicit ClockSaveSection(Core::SimClock& cl)
		: clock(cl)
	{}

	U64 getId() const override { return saveHash(SECTION_NAME); }
	std::string_view getName() const override { return SECTION_NAME; }
	U32 getVersion() const override { return CURRENT_VERSION; }

	void write(SectionWriteContext& ctx) override;
	void read(SectionReadContext& ctx) override;

private:
	Core::SimClock& clock;
};

} // namespace Blackthorn::Saves::Sections