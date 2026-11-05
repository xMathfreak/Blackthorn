#pragma once

#include "Core/Export.hpp"
#include "Debug/Logger.hpp"

namespace Blackthorn::Debug {

struct BLACKTHORN_API DebugConfig {
	float profilingLogInterval = 1.0f;
	LoggerConfig logger;
};

} // namespace Blackthorn::Debug