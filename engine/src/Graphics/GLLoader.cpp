#include "Graphics/GLLoader.h"

#include <glad/gl.h>
#include <SDL3/SDL.h>

#include "Debug/Logger.h"

namespace Blackthorn::Graphics {

bool loadGLFunctions() {
	if (gladLoadGL(SDL_GL_GetProcAddress) == 0) {
		BT_ERROR("Graphics::loadGLFunctions: gladLoadGLLoader failed");
		return false;
	}

	BT_DEBUG("Graphics::loadGLFunctions: GL functions loaded inside BlackthornEngine");
	return true;
}

} // namespace Blackthorn::Graphics