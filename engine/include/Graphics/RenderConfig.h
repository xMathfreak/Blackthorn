#pragma once

#include "Core/Export.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::Graphics {

struct BLACKTHORN_API RenderConfig {
	int openglMajor = 3;
	int openglMinor = 3;
	int depthBits = 16;
	int stencilBits = 0;

	/// Maximum number of quads per batch.
	/// Drives MAX_VERTICES `(maxQuads * 4)` and
	/// MAX_INDICES `(maxQuads * 6)` inside the Renderer.
	U32 maxQuads = 4096;

	static constexpr U32 maxTextureSlots = 16;
};

} // namespace Blackthorn::Graphics