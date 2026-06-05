#pragma once

#include "Core/Export.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::Audio {

struct BLACKTHORN_API AudioConfig {
	size_t maxVoices = 32;
	double streamingThreshold = 4 * 1024 * 1024;
};

};