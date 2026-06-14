#pragma once

#include <string>

#include "Core/Export.h"

namespace Blackthorn {

struct BLACKTHORN_API MetadataConfig {
	std::string name = "Blackthorn App";
	std::string version = "1.0.0";
	std::string identifier = "blackthorn.app";
	std::string author = "";
	std::string copyright = "";
	std::string url = "";
	std::string type = "game";
};

} // namespace Blackthorn