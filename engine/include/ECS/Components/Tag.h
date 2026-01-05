#pragma once

#include <string>

#include "Core/Export.h"

namespace Blackthorn::ECS::Components {

struct BLACKTHORN_API Tag {
	std::string name;

	BLACKTHORN_API Tag() = default;
	BLACKTHORN_API Tag(const std::string& tag) : name(tag) {}
};

} // namespace Blackthorn::ECS::Components