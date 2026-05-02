#include "ECS/Detail.h"

#include <atomic>
#include <mutex>
#include <typeindex>
#include <unordered_map>

namespace Blackthorn::ECS::Detail {

size_t componentIDForType(std::type_index type) noexcept {
	static std::atomic<size_t> counter{0};
	static std::mutex mutex;
	static std::unordered_map<std::type_index, size_t> registry;

	{
		std::lock_guard<std::mutex> lock(mutex);
		auto it = registry.find(type);
		if (it != registry.end())
			return it->second;

		const size_t id = counter.fetch_add(1, std::memory_order_relaxed);
		registry.emplace(type, id);
		return id;
	}
}

} // namespace Blackthorn::ECS::Detail