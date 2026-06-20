#pragma once

#include <string_view>
#include <unordered_map>

#include "ECS/Detail.h"
#include "ECS/EntityPool.h"
#include "Inspector/ComponentInspector.h"

namespace Blackthorn::Editor::Inspector {

template <typename T>
concept HasInspectorSpecialization = requires(T& t) {
	{ ComponentInspector<T>::draw(t) } -> std::same_as<bool>;
};

class InspectorRegistry {
public:
	using ConstructFn = void(*)(ECS::EntityPool&, ECS::Entity);
	using DestoryFn = void(*)(ECS::EntityPool&, ECS::Entity);
	using DrawFn = bool(*)(void*);

	struct Entry {
		std::string_view name;
		DrawFn draw = nullptr;
		ConstructFn construct = nullptr;
		DestoryFn destroy = nullptr;
	};

public:
	static InspectorRegistry& instance();

	InspectorRegistry(const InspectorRegistry&) = delete;
	InspectorRegistry& operator=(const InspectorRegistry&) = delete;

	template <HasInspectorSpecialization T>
	void registerInspectorItem() {
		const size_t id = ECS::Detail::componentID<T>();

		auto [it, inserted] = entries.try_emplace(id);
		if (!inserted)
			return;

		Entry& entry = it->second;

		entry.name = ComponentInspector<T>::name();

		entry.draw = [](void* comp) -> bool {
			return ComponentInspector<T>::draw(
				*static_cast<T*>(comp)
			);
		};

		entry.construct = [](ECS::EntityPool& pool, ECS::Entity e) {
			pool.addComponent<T>(e);
		};

		entry.destroy = [](ECS::EntityPool& pool, ECS::Entity e) {
			pool.removeComponent<T>(e);
		};
	}

	const Entry* getEntry(size_t componentID) const {
		auto it = entries.find(componentID);
		return it != entries.end() ? &it->second : nullptr;
	}

	bool isRegistered(size_t componentID) const {
		return entries.count(componentID) > 0;
	}

private:
	InspectorRegistry() = default;

	std::unordered_map<size_t, Entry> entries;
};

} // namespace Blackthorn::Editor::Inspector