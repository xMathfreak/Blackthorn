#pragma once

#include <typeinfo>

#include "Core/Export.hpp"

namespace Blackthorn::Assets::Detail {

BLACKTHORN_API const void* getTypeKey(const std::type_info& type);

template <typename T>
const void* typeKey() {
	return getTypeKey(typeid(T));
}

} // namespace Blackthorn::Assets::Detail