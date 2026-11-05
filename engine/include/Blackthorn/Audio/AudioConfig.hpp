#pragma once

#include "Blackthorn/Core/Export.hpp"
#include "Blackthorn/Types/Numeric.hpp"

namespace Blackthorn::Audio {

struct BLACKTHORN_API AudioConfig {
	size_t maxVoices = 32;
	double streamingThreshold = 4 * 1024 * 1024;
};

};