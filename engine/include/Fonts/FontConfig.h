#pragma once

#include "Core/Export.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::Fonts {

struct BLACKTHORN_API FontConfig {
	U32 maxCachedText = 256;
	U32 maxTextGlyphs = 2048;
	int atlasSize = 1024;
	U32 tabSpaces = 4;

	static void setCurrent(const FontConfig& cfg);
	static const FontConfig& getCurrent();

private:
	static FontConfig current;
};

} // namespace Blackthorn::Fonts