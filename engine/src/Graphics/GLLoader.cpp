#include "Graphics/GLLoader.h"

#include <glad/glad.h>
#include <SDL3/SDL.h>

#include "Debug/Logger.h"

namespace Blackthorn::Graphics {

bool loadGLFunctions() {
	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress))) {
		BT_ERROR("Graphics::loadGLFunctions: gladLoadGLLoader failed");
		return false;
	}

	BT_DEBUG("Graphics::loadGLFunctions: GL functions loaded inside BlackthornEngine");
	return true;
}

} // namespace Blackthorn::Graphics