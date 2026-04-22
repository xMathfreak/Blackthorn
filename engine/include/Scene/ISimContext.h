#pragma once

#include "Core/Export.h"

namespace Blackthorn {

namespace Assets { class AssetManager; }
namespace Core { class SimClock; }
namespace Jobs { class JobSystem; }
namespace Net { class ConnectionManager; }

namespace Scene {

class SceneManager;

/**
 * @interface ISimContext
 * @brief Minimal simulation context shared by both client and headless server.
 *
 * @details
 * `EngineCore` implements this interface and provides access to core
 * simulation services such as ECS, asset management, job system and
 * the simulation clock. It intentionally excludes any rendering or
 * presentation-specific functionality so it can be used in headless builds.
 *
 * Scenes that depend only on simulation should accept an `ISimContext&`
 * rather than a higher-level context. This allows the same scene code to
 * compile and run unchanged on both client and dedicated server targets.
 *
 * Scenes that require rendering or presentation features should instead
 * accept `ISceneContext&` (which extends `ISimContext`), or perform a
 * runtime check:
 * @code
 * if (auto* renderCtx = dynamic_cast<ISceneContext*>(&ctx)) {
 *     // safe to use rendering features
 * }
 * @endcode
 *
 * This separation keeps simulation logic portable while allowing optional
 * rendering paths when available.
 */
class BLACKTHORN_API ISimContext {
public:
	virtual ~ISimContext() = default;

	virtual Assets::AssetManager& getAssetManager() = 0;
	virtual SceneManager& getSceneManager() = 0;
	virtual Jobs::JobSystem& getJobSystem() = 0;
	virtual Core::SimClock& getSimClock() = 0;
	virtual Net::ConnectionManager& getConnectionManager() = 0;
};

} // namespace Scene

} // namespace Blackthorn