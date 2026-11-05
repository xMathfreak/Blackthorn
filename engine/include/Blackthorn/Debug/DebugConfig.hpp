#pragma once

#include "Blackthorn/Core/Export.hpp"
#include "Blackthorn/Debug/Logger.hpp"

namespace Blackthorn::Debug {

struct BLACKTHORN_API DebugConfig {
	float profilingLogInterval = 1.0f;
	LoggerConfig logger;
};

} // namespace Blackthorn::Debug