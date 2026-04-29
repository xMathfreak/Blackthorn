#pragma once

#include <string>
#include <string_view>

#include "Core/Export.h"
#include "Saves/SaveHash.h"
#include "Saves/Sections/ISaveSection.h"

namespace Blackthorn::Saves::Sections {

/**
 * @brief Built-in save section for engine metadata (@c bt.meta).
 *
 * Stores human-readable and machine-readable metadata about the save that
 * is useful for the save listing UI, migration checks, and diagnostics.
 * All fields are informational, none affect game simulation.
 *
 * @par Wire layout (payload)
 * @code
 * [uint16 engineFormatVersion]
 * [uint32 totalPlaytimeTicks]  # cumulative ticks since the save was created
 * [string engineVersionString] # e.g. "Blackthorn 0.1.0"
 * [string platform]            # e.g. "Linux x86_64", "Windows x64"
 * @endcode
 *
 * @par Version history
 * - Version 1: Initial layout.
 */
class BLACKTHORN_API MetaSaveSection final : public ISaveSection {
public:
	static constexpr std::string_view SECTION_NAME = "bt.meta";
	static constexpr U32 CURRENT_VERSION = 1;

	/**
	 * @brief Constructs a meta section.
	 *
	 * @param version Engine version string written into the section.
	 * @param playTicks Total ticks elapsed since save creation. Updated on each
	 *                  write so it accumulates over multiple sessions.
	 */
	MetaSaveSection(std::string version, U32 playTicks = 0)
		: engineVersion(std::move(version))
		, playtimeTicks(playTicks)
	{}

	U64 getId() const override { return saveHash(SECTION_NAME); }
	std::string_view getName() const override { return SECTION_NAME; }
	U32 getVersion() const override { return CURRENT_VERSION; }

	void write(SectionWriteContext& ctx) override;
	void read(SectionReadContext& ctx) override;

	const std::string& getEngineVersion() const noexcept { return engineVersion; }
	const std::string& getPlatform() const noexcept { return platform; }
	U32 getPlaytimeTicks() const noexcept { return playtimeTicks; }

	void setPlaytimeTicks(U32 ticks) noexcept { playtimeTicks = ticks; }

private:
	std::string engineVersion;
	std::string platform;
	U32 playtimeTicks = 0;

	static std::string currentPlatformString();
};

} // namespace Blackthorn::Saves::Sections