#pragma once

#include "Core/Export.h"

namespace Blackthorn::Graphics {

/**
 * @brief Loads OpenGL function pointers for this binary via GLAD.
 *
 * @details
 * GLAD's generated function pointers are file-scope globals. When GLAD is
 * statically linked into multiple binary images, for example once into
 * BlackthornEngine (a shared library) and again into a host application such
 * as an editor, each image gets its own independent, zero-initialized copy
 * of those globals. Calling gladLoadGLLoader() from the host only populates
 * the host's copy; it does not initialize the copy used by this library.
 *
 * Call this once, on the thread with the current OpenGL context, before
 * constructing any Blackthorn::Graphics object (Renderer, Texture, Shader,
 * etc.) when the host application creates and manages its own GL context.
 * Engine::initGraphics() performs the equivalent loading internally, so
 * applications using Engine directly do not need to call this function.
 *
 * @return true if the OpenGL function pointers were loaded successfully.
 */
BLACKTHORN_API bool loadGLFunctions();

} // namespace Blackthorn::Graphics