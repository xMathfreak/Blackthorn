#include "ECS/Detail.h"

#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace Blackthorn::ECS::Detail {

namespace {

struct ComponentRegistry {
	std::mutex mutex;
	std::unordered_map<std::type_index, size_t> ids;
};

ComponentRegistry& registry() {
	// Function-local static: constructed on first use, so registration from
	// other static initializers is safe.
	static ComponentRegistry instance;
	return instance;
}

} // namespace

size_t assignComponentID(std::type_index type) {
	ComponentRegistry& reg = registry();
	std::lock_guard<std::mutex> lock(reg.mutex);

	if (const auto it = reg.ids.find(type); it != reg.ids.end())
		return it->second;

	if (reg.ids.size() >= MAX_COMPONENTS) {
		throw std::length_error(
			"ECS: component type limit (" + std::to_string(MAX_COMPONENTS) + ") exceeded");
	}

	const size_t id = reg.ids.size();
	reg.ids.emplace(type, id);
	return id;
}

} // namespace Blackthorn::ECS::Detail