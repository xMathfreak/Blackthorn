#include "Saves/Sections/MetaSaveSection.h"

#include "Saves/SaveDocument.h"

namespace Blackthorn::Saves::Sections {

void MetaSaveSection::write(SectionWriteContext& ctx) {
	ctx.buffer.writeU16(SAVE_FORMAT_VERSION);
	ctx.buffer.writeU32(playtimeTicks);
	ctx.buffer.writeString(engineVersion);
	ctx.buffer.writeString(currentPlatformString());
}

void MetaSaveSection::read(SectionReadContext& ctx) {
	[[maybe_unused]] const U16 formatVersion = ctx.buffer.readU16();

	playtimeTicks = ctx.buffer.readU32();
	engineVersion = ctx.buffer.readString();
	platform = ctx.buffer.readString();
}

std::string MetaSaveSection::currentPlatformString() {
#if defined(_WIN32)
	#if defined(_WIN64)
		return "Windows x64";
	#else
		return "Windows x86";
	#endif
#elif defined(__APPLE__)
	#include <TargetConditionals.h>
	#if TARGET_OS_MAC
		return "macOS";
	#else
		return "Apple (unknown)";
	#endif
#elif defined(__linux__)
	#if defined(__x86_64__)
		return "Linux x86_64";
	#elif defined(__aarch64__)
		return "Linux arm64";
	#else
		return "Linux";
	#endif
#else
	return "Unknown";
#endif
}

} // namespace Blackthorn::Saves::Sections