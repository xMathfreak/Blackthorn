#pragma once

#include <string>

#include "Core/Export.h"

namespace Blackthorn::ECS::Components {

/// Human-readable entity name. Unique among live entities when
/// set through scripts.
struct BLACKTHORN_API Name {
	std::string value;
};

} // namespace Blackthorn::ECS::Components