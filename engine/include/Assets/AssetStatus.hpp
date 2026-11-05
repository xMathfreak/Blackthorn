#pragma once

#include "Types/Numeric.h"

namespace Blackthorn::Assets {

/**
 * @brief Lifecycle state of an asset.
 *
 * - Missing: never loaded, unloaded, or a null ref.
 * - Pending: an async load is queued or in flight.
 * - Ready:   resolve() returns a valid pointer.
 * - Failed:  the most recent load failed. Load again to retry.
 */
enum class AssetStatus : U8 {
	Missing,
	Pending,
	Ready,
	Failed
};

} // namespace Blackthorn::Assets