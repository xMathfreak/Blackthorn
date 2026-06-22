#pragma once

#include <imgui.h>
#include <SDL3/SDL.h>

#include <Core/Engine.h>
#include <Graphics/Renderer.h>

#include "Panels/AssetBrowser.h"
#include "Panels/AssetInspector.h"
#include "Panels/Dockspace.h"
#include "Panels/Hierarchy.h"
#include "Panels/Inspector.h"
#include "Panels/TitleBar.h"
#include "Panels/Viewport.h"

#include "State/Simulation.h"

namespace Blackthorn::Editor {

class Application {
public:
	Application() = default;
	~Application();

	Application(const Application&) = delete;
	Application& operator=(const Application&) = delete;

	bool init();
	void run();

private:
	void render();
	void update();
	void processEvents();
	void shutdown();
	void stepSimulation();
	void initAssetLoaders();
	void showImportDialog();

	static bool handleLiveResize(void* userdata, SDL_Event* event);
	static void handleImportDialog(void* userdata, const char* const* filelist, int filter);

private: // Panels
	Panels::AssetBrowser assetBrowser;
	Panels::AssetInspector assetInspector;
	Panels::Dockspace dockspace;
	Panels::Hierarchy hierarchy;
	Panels::Inspector inspector;
	Panels::TitleBar titleBar;
	Panels::Viewport viewport;

private: // State
	State::Context context;
	State::Dockspace dockspaceState;
	State::Simulation simulationState;
	State::Titlebar titleBarState;
	State::Viewport viewportState;

private:
	std::vector<SDL_DialogFileFilter> importFilters;
	std::string importFilterPattern;

	TimingConfig timingConfig;

	std::unique_ptr<EngineCore> engine;
	std::unique_ptr<Graphics::Renderer> renderer;

	ECS::World world;

	SDL_Window* window = nullptr;
	SDL_GLContext glContext = nullptr;

	std::unique_ptr<Audio::AudioManager> audioManager;

	bool initialized = false;
	bool running = false;
	bool frameInProgress = false;

	ImFont* zekton24 = nullptr;
	Graphics::Texture texture;
};

} // namespace Blackthorn