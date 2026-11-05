#pragma once

#include <cstddef>

#include "Core/Export.h"

namespace Blackthorn::Assets {

struct BLACKTHORN_API AssetConfig {
	/// Maximum number of async asset uploads per frame.
	size_t uploadBudget = 4;
};

} // namespace Blackthorn::Assets