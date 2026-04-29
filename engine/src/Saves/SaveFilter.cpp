#include "Saves/SaveFilter.h"

namespace Blackthorn::Saves {

bool SaveFilter::matches(const SaveId& id) const noexcept {
	if (worldId.has_value() && id.worldId != *worldId)
		return false;

	if (playerId.has_value() && id.playerId != *playerId)
		return false;

	if (slot.has_value() && id.slot != *slot)
		return false;

	if (flags.has_value()) {
		const U32 required = static_cast<U32>(*flags);
		const U32 actual   = static_cast<U32>(id.flags);

		if ((actual & required) != required)
			return false;
	}

	if (createdAfter.has_value() && id.createdAt < *createdAfter)
		return false;

	if (createdBefore.has_value() && id.createdAt > *createdBefore)
		return false;

	return true;
}

} // namespace Blackthorn::Saves