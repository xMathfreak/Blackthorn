#pragma once

#include "Assets/AssetEntry.h"

namespace Blackthorn::Editor {

/**
 * @brief Specialize per asset type to draw a type specific inspector view.
 */
template <typename T>
struct AssetInspector {};

} // namespace Blackthorn::Editor