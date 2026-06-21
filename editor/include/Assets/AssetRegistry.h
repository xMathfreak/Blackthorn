#pragma once

#include <string>
#include <string_view>
#include <typeindex>
#include <vector>

#include "Assets/AssetManager.h"

namespace Blackthorn::Editor::Assets {

/**
 * @brief Type-erased entry describing one asset type the editor knows how
 * to browse, classify, and load.
 *
 * Mirrors InspectorRegistry / SerializerRegistry: editor code interacts
 * with assets purely through this registry, never needing compile-time
 * knowledge of which concrete asset types exist.
 */
class AssetRegistry {
public:
	using LoadFn = void*(*)(Blackthorn::Assets::AssetManager&, const std::string& id, const std::string& path);

	struct Entry {
		std::type_index type = std::type_index(typeid(void));
		std::string_view name;
		std::vector<std::string> extensions;
		LoadFn load = nullptr;
	};

	static AssetRegistry& instance();

	AssetRegistry(const AssetRegistry&) = delete;
	AssetRegistry& operator=(const AssetRegistry&) = delete;

	template <typename T>
	void registerAssetType(std::string_view name, Blackthorn::Assets::AssetManager& manager) {
		const std::type_index type = std::type_index(typeid(T));

		for (const auto& existing : entries) {
			if (existing.type == type)
				return;
		}

		Entry entry{};
		entry.type = type;
		entry.name = name;
		entry.extensions = manager.getSupportedExtensions<T>();

		entry.load = [](Blackthorn::Assets::AssetManager& mgr, const std::string& id, const std::string& path) -> void* {
			auto handle = mgr.load<T>(id, path);
			return handle ? handle.get() : nullptr;
		};

		entries.push_back(std::move(entry));
	}

	const Entry* getEntry(std::type_index type) const {
		for (const auto& e : entries)
			if (e.type == type)
				return &e;
		return nullptr;
	}

	const Entry* findByExtension(const std::string& ext) const {
		for (const auto& e : entries) {
			for (const auto& known : e.extensions) {
				if (known == ext)
					return &e;
			}
		}
		return nullptr;
	}

	const std::vector<Entry>& allEntries() const { return entries; }

private:
	AssetRegistry() = default;

	std::vector<Entry> entries;
};

} // namespace Blackthorn::Editor::Assets