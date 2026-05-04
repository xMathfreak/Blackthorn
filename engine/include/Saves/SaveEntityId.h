#pragma once

#include "Core/Types/Numeric.h"

namespace Blackthorn::Saves {

/**
 * @brief Stable numeric identity for a persistent ECS entity across save sessions.
 */
using SaveEntityId = U64;

static constexpr SaveEntityId INVALID_SAVE_ENTITY = U64_MAX;

} // namespace Blackthorn::Saves