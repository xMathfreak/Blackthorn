#include "Assets/TypeKey.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Blackthorn::Assets::Detail {

const void* getTypeKey(const std::type_info& type) {
	static std::mutex mutex;
	static std::unordered_map<std::string, std::unique_ptr<unsigned char>> keys;

	const std::string name = type.name();

	std::lock_guard lock(mutex);

	auto it = keys.find(name);

	if (it != keys.end())
		return it->second.get();

	auto key = std::make_unique<unsigned char>(0);
	const auto* result = key.get();

	keys.emplace(name, std::move(key));

	return result;
}

} // namespace Blackthorn::Assets::Detail