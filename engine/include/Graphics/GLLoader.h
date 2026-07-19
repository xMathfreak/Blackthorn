#pragma once

#include "Core/Export.h"

namespace Blackthorn::Graphics {

/**
 * @brief Loads OpenGL function pointers for this binary via GLAD.
 *
 * @details
 * GLAD's generated function pointers are file-scope globals. When GLAD is
 * statically linked into more than one binary image - e.g. once into
 * `BlackthornEngine` (a shared library) and again into a host application
 * such as the editor - each image gets its own independent, separately
 * zero-initialized copy of those globals. Calling `gladLoadGLLoader()` from
 * the host application only populates the host's own copy; it has no effect
 * on the copy compiled into this library.
 *
 * Call this once, on the thread holding the current OpenGL context, before
 * constructing any `Blackthorn::Graphics` object (`Renderer`, `Texture`,
 * `Shader`, etc.) from a host application that sets up its own GL context.
 * `Engine::initGraphics()` already performs the equivalent loading
 * internally, so client builds using `Engine` directly never need this.
 *
 * @return true on success.
 */
BLACKTHORN_API bool loadGLFunctions();

} // namespace Blackthorn::Graphics