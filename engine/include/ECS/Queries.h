#pragma once

#include <algorithm>
#include <string_view>
#include <vector>

#include "ECS/Components/Tag.h"
#include "ECS/Detail.h"
#include "ECS/EntityPool.h"

namespace Blackthorn::ECS {

/**
 * @brief Every entity in @p pool carrying the tag @p tag, sorted by slot index.
 *
 * Sorting makes results deterministic across runs. Removals reorder the dense
 * component storage, so the raw scan order is not stable.
 */
inline std::vector<Entity> findEntitiesWithTag(const EntityPool& pool, std::string_view tag) {
	std::vector<Entity> found = pool.collectEntities<Components::Tag>(
		[tag](const Components::Tag& t) { return t.has(tag); });

	std::sort(found.begin(), found.end(), [](Entity a, Entity b) {
		return Detail::entityIndex(a) < Detail::entityIndex(b);
	});
	return found;
}

} // namespace Blackthorn::ECS