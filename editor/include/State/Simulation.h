#pragma once

#include "Core/Types/Numeric.h"

namespace Blackthorn::Editor::State {

struct Simulation {
	U64 tick = 0;
	U64 lastPerfCounter = 0;

	float accumulated = 0.0f;
	float alpha = 1.0f;

	bool playing = true;
};

} // namespace Blackthorn::Editor::Panels