#pragma once

#include "Blackthorn/Types/Numeric.hpp"

namespace Blackthorn::Saves {

/**
 * @brief Stable numeric identity for a persistent ECS entity across save sessions.
 */
using SaveEntityID = U64;

static constexpr SaveEntityID INVALID_SAVE_ENTITY = U64_MAX;

} // namespace Blackthorn::Saves