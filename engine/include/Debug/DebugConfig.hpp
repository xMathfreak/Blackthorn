#pragma once

#include "Core/Export.h"
#include "Debug/Logger.h"

namespace Blackthorn::Debug {

struct BLACKTHORN_API DebugConfig {
	float profilingLogInterval = 1.0f;
	LoggerConfig logger;
};

} // namespace Blackthorn::Debug