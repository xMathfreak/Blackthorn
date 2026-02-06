#pragma once

#include <cstddef>

namespace Blackthorn::Text {

enum class Alignment {
	Left,
	Center,
	Right
};

struct Metrics {
	float width;
	float height;
	size_t lineCount;
};

} // namespace Blackthorn::Fonts