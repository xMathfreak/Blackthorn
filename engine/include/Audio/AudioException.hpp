#pragma once

#include <stdexcept>
#include "Core/Export.h"

namespace Blackthorn::Audio {

class BLACKTHORN_API AudioException final : public std::runtime_error {
public:
	explicit AudioException(const std::string& message)
		: std::runtime_error(message)
	{}
};

void checkOpenALError(const char* context = "");

} // namespace Blackthorn::Audio