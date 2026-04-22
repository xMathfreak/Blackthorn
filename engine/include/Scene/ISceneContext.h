#pragma once

#include "Core/Export.h"
#include "Scene/ISimContext.h"

namespace Blackthorn {

namespace Graphics { class Renderer; }
namespace Input { class InputManager; }

namespace Scene {

/**
 * @interface ISceneContext
 * @brief Full scene context available to client-side (graphics-enabled) scenes.
 *
 * @details
 * Extends `ISimContext` with rendering and presentation capabilities,
 * including access to the `Renderer`. This interface is implemented by
 * `Engine`, the client build that includes graphics support.
 *
 * Scenes that only depend on simulation services should prefer
 * `ISimContext&` to remain portable across both client and dedicated
 * server targets. Scenes that require rendering should instead accept
 * `ISceneContext&`.
 *
 * For scenes written against `ISimContext&` that optionally use rendering,
 * a runtime check can be performed:
 *
 * @code
 * if (auto* full = dynamic_cast<ISceneContext*>(&context)) {
 *     full->getRenderer().drawQuad(...);
 * }
 * @endcode
 *
 * This pattern allows a single scene implementation to support both
 * headless and graphical execution paths without compile-time branching.
 */
class BLACKTHORN_API ISceneContext : public ISimContext {
public:
	virtual ~ISceneContext() = default;

	virtual Graphics::Renderer& getRenderer() = 0;
	virtual Input::InputManager& getInputManager() = 0;
};

} // namespace Scene

} // namespace Blackthorn