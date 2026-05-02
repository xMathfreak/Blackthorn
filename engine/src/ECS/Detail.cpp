#include "ECS/Detail.h"

#include <mutex>
#include <typeindex>
#include <unordered_map>

namespace Blackthorn::ECS::Detail {

size_t componentIDForType(std::type_index type) noexcept {
	static size_t counter = 0;
	static std::mutex mutex;
	static std::unordered_map<std::type_index, size_t> registry;

	std::lock_guard lock(mutex);

	auto [it, inserted] = registry.emplace(type, counter);
	if (inserted)
		++counter;

	return it->second;
}

} // namespace Blackthorn::ECS::Detail