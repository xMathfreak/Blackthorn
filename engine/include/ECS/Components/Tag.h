#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "Core/Export.h"

namespace Blackthorn::ECS::Components {

/**
 * @brief Group labels on an entity. Values are unique within one entity.
 *
 * Mutate only through add() and remove(), which keep the list unique. An
 * entity with no tags should not hold this component. The scripting layer
 * removes it when the last tag goes.
 */
struct BLACKTHORN_API Tag {
	std::vector<std::string> values;

	[[nodiscard]]
	bool has(std::string_view tag) const {
		return std::any_of(values.begin(), values.end(),
			[tag](const std::string& v) { return v == tag; }
		);
	}

	/// @return false if the tag was already present.
	bool add(std::string_view tag) {
		if (has(tag))
			return false;

		values.emplace_back(tag);
		return true;
	}

	/// @return false if the tag was not present.
	bool remove(std::string_view tag) {
		const auto it = std::find_if(values.begin(), values.end(),
			[tag](const std::string& v) { return v == tag; }
		);

		if (it == values.end())
			return false;

		values.erase(it);
		return true;
	}
};

} // namespace Blackthorn::ECS::Components