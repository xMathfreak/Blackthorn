#pragma once

#include "Graphics/Renderer.h"
#include "Scene/ISceneContext.h"

namespace Blackthorn::Scene {

/**
 * @brief Concrete `ISceneContext` implementation owned by `Engine`.
 *
 * Extends `ISceneContext` (which extends `ISimContext`) with renderer
 * access. Only instantiated in the graphics-enabled client build - the
 * server uses `SimContextImpl` directly via `ISimContext`.
 */
class SceneContextImpl : public ISceneContext {
	Audio::AudioManager& audio;
	Assets::AssetManager& assets;
	Net::ConnectionManager& connection;
	Input::InputManager& input;
	Jobs::JobSystem& jobs;
	SceneManager& scene;
	Core::SimClock& simClock;
	Graphics::Renderer& renderer;
	Saves::SaveManager& saves;

public:
	SceneContextImpl(
		Audio::AudioManager& au,
		Assets::AssetManager& am,
		Net::ConnectionManager& cm,
		Input::InputManager& im,
		Jobs::JobSystem& js,
		SceneManager& sm,
		Core::SimClock& clock,
		Graphics::Renderer& ren,
		Saves::SaveManager& sv
	)
		: audio(au)
		, assets(am)
		, connection(cm)
		, input(im)
		, jobs(js)
		, scene(sm)
		, simClock(clock)
		, renderer(ren)
		, saves(sv)
	{}

	Audio::AudioManager& getAudioManager() override { return audio; }
	Assets::AssetManager& getAssetManager() override { return assets; }
	Net::ConnectionManager& getConnectionManager() override { return connection; }
	Input::InputManager& getInputManager() override { return input; }
	Jobs::JobSystem& getJobSystem() override { return jobs; }
	SceneManager& getSceneManager() override { return scene; }
	Core::SimClock& getSimClock() override { return simClock; }
	Graphics::Renderer& getRenderer() override { return renderer; }
	Saves::SaveManager& getSaveManager() override { return saves; }
};

} // namespace Blackthorn::Scene