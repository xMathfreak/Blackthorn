#include "Editor.h"

#include <glad/glad.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>

#include "Debug/Logger.h"
#include "Graphics/GLLoader.h"

#include "Inspector/InspectorRegistry.h"
#include "Inspector/AudioPreviewContext.h"

#include "Inspector/Components/Transform.h"
#include "Inspector/Components/Sprite.h"
#include "Inspector/Components/Kinematics.h"
#include "Inspector/Components/Persistent.h"

#include "Inspector/AssetInspectors/Texture.h"
#include "Inspector/AssetInspectors/AudioClip.h"
#include "Inspector/AssetInspectors/Shader.h"

#include "Assets/Loaders/AudioLoader.h"
#include "Assets/Loaders/TextureLoader.h"
#include "Assets/Loaders/ShaderLoader.h"
#include "Assets/Loaders/BitmapFontLoader.h"
#include "Assets/Loaders/TrueTypeFontLoader.h"

#include "ECS/Systems/RenderSystem.h"

namespace Blackthorn::Editor {

Application::~Application() {
	shutdown();
}

bool Application::init() {
	if (initialized)
		return true;

	EngineConfig cfg;
	cfg.metadata.name = "Blackthorn Editor";
	cfg.metadata.identifier = "blackthorn.editor";
	cfg.metadata.type = "application";

	timingConfig = cfg.timing;

	engine = std::make_unique<EngineCore>();
	if (!engine->init(cfg))
		return false;

	if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
		return false;

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	window = SDL_CreateWindow(
		"Blackthorn Editor",
		800,
		600,
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS
	);

	if (!window) {
		SDL_Quit();
		return false;
	}

	glContext = SDL_GL_CreateContext(window);
	if (!glContext) {
		SDL_Quit();
		return false;
	}

	if (!SDL_GL_MakeCurrent(window, glContext)) {
		SDL_Quit();
		return false;
	}

	SDL_SetWindowMinimumSize(window, 800, 600);
	SDL_GL_SetSwapInterval(-1);

	if (!Graphics::loadGLFunctions())
		return false;

	if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
		return false;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	zekton24 =
		io.Fonts->AddFontFromFileTTF(
			"assets/fonts/Zekton-Regular.otf",
			24.0f
		);

	renderer = std::make_unique<Graphics::Renderer>();

	initAssetLoaders();

	audioManager = std::make_unique<Audio::AudioManager>();
	if (!audioManager->init())
		BT_ERROR("Editor: failed to initialize AudioManager");

	AudioPreviewContext::setManager(audioManager.get());

	auto& assetRegistry = Assets::AssetRegistry::instance();
	assetRegistry.registerAssetType<Graphics::Texture>("Texture", engine->getAssetManager());
	assetRegistry.registerAssetType<Audio::AudioClip>("Audio Clip", engine->getAssetManager());
	assetRegistry.registerFileType<Graphics::Shader>("Shader", { ".glsl", ".vert", ".frag" });
	assetRegistry.registerAssetType<Fonts::BitmapFont>("Bitmap Font", engine->getAssetManager());
	assetRegistry.registerAssetType<Fonts::TrueTypeFont>("TrueType Font", engine->getAssetManager());

	for (const auto& entry : assetRegistry.allEntries()) {
		for (const auto& ext : entry.extensions) {
			if (!importFilterPattern.empty())
				importFilterPattern += ';';

			importFilterPattern += ext.substr(1);
		}
	}

	importFilters.push_back({ "All Supported Assets", importFilterPattern.c_str() });
	importFilters.push_back({ "All Files", "*" });

	ImGui::StyleColorsDark();

	ImGui_ImplSDL3_InitForOpenGL(window, glContext);
	ImGui_ImplOpenGL3_Init("#version 330");

	int w, h;
	SDL_GetWindowSizeInPixels(window, &w, &h);
	glViewport(0, 0, w, h);
	renderer->setProjection(w, h);

	auto hitTest = [](SDL_Window* win, const SDL_Point* area, void* data) -> SDL_HitTestResult {
		auto* self = static_cast<Application*>(data);

		if (!(SDL_GetWindowFlags(win) & SDL_WINDOW_MAXIMIZED)) {
			int w, h;
			SDL_GetWindowSizeInPixels(win, &w, &h);

			constexpr int border = 8;

			bool left = area->x < border;
			bool right = area->x >= w - border;
			bool top = area->y < border;
			bool bottom = area->y >= h - border;

			if (top && left)
				return SDL_HITTEST_RESIZE_TOPLEFT;

			if (top && right)
				return SDL_HITTEST_RESIZE_TOPRIGHT;

			if (bottom && left)
				return SDL_HITTEST_RESIZE_BOTTOMLEFT;

			if (bottom && right)
				return SDL_HITTEST_RESIZE_BOTTOMRIGHT;

			if (left)
				return SDL_HITTEST_RESIZE_LEFT;

			if (right)
				return SDL_HITTEST_RESIZE_RIGHT;

			if (top)
				return SDL_HITTEST_RESIZE_TOP;

			if (bottom)
				return SDL_HITTEST_RESIZE_BOTTOM;
		}

		if (area->y >= static_cast<int>(self->titleBarState.height))
			return SDL_HITTEST_NORMAL;

		for (const SDL_Rect& rect : self->titleBarState.hitExclusionRects) {
			if (SDL_PointInRect(area, &rect))
				return SDL_HITTEST_NORMAL;
		}

		return SDL_HITTEST_DRAGGABLE;
	};

	SDL_SetWindowHitTest(window, hitTest, this);
	SDL_AddEventWatch(&Application::handleLiveResize, this);

	auto& inspector = Inspector::InspectorRegistry::instance();

	inspector.registerInspectorItem<ECS::Components::Transform>();
	inspector.registerInspectorItem<ECS::Components::Sprite>();
	inspector.registerInspectorItem<ECS::Components::Kinematics>();
	inspector.registerInspectorItem<ECS::Components::Persistent>();


	context.activeWorld = &world;
	context.activeWorld->addSystem<ECS::Systems::RenderSystem>(renderer.get());
	initialized = true;
	return true;
}

void Application::initAssetLoaders() {
	auto& assets = engine->getAssetManager();

	assets.registerLoader<Audio::AudioClip>(
		std::make_unique<Audio::AudioLoader>(),
		std::make_unique<Audio::AsyncAudioLoader>()
	);

	assets.registerLoader<Graphics::Texture>(
		std::make_unique<Graphics::TextureLoader>(),
		std::make_unique<Graphics::AsyncTextureLoader>()
	);

	assets.registerLoader<Graphics::Shader>(std::make_unique<Graphics::ShaderLoader>());
	assets.registerLoader<Fonts::BitmapFont>(std::make_unique<Fonts::BitmapFontLoader>());
	assets.registerLoader<Fonts::TrueTypeFont>(std::make_unique<Fonts::TrueTypeFontLoader>());
}

void Application::shutdown() {
	if (!initialized)
		return;

	SDL_RemoveEventWatch(&Application::handleLiveResize, this);

	audioManager.reset();
	renderer.reset();

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();

	SDL_GL_DestroyContext(glContext);
	SDL_DestroyWindow(window);
	SDL_Quit();

	initialized = false;
	running = false;
}

void Application::run() {
	if (!initialized)
		return;

	running = true;

	while (running) {
		processEvents();
		update();
		render();
	}
}

void Application::processEvents() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		ImGui_ImplSDL3_ProcessEvent(&event);

		switch (event.type) {
			case SDL_EVENT_QUIT:
				running = false;
				break;

			case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
				SDL_Window* closedWindow = SDL_GetWindowFromID(event.window.windowID);

				if (closedWindow == window)
					running = false;

				break;
			}
		}
	}
}

void Application::render() {
	frameInProgress.store(true, std::memory_order::relaxed);

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	Inspector::AssetPickerContext::set(&context.assetCache, &engine->getAssetManager());

	titleBar.draw(window, running, titleBarState, context);
	dockspace.draw(titleBarState, dockspaceState, context, running);
	hierarchy.draw(context);
	inspector.draw(context);
	assetBrowser.draw(context, engine->getAssetManager());
	assetInspector.draw(context, engine->getAssetManager());
	viewport.draw(context, *renderer, viewportState, simulationState.alpha);

	ImGui::Render();

	int w, h;
	SDL_GetWindowSizeInPixels(window, &w, &h);
	glViewport(0, 0, w, h);

	glClear(GL_COLOR_BUFFER_BIT);

	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		SDL_Window* backupWindow = SDL_GL_GetCurrentWindow();
		SDL_GLContext backupContext = SDL_GL_GetCurrentContext();

		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();

		SDL_GL_MakeCurrent(backupWindow, backupContext);
	}

	SDL_GL_SwapWindow(window);
	frameInProgress.store(false, std::memory_order::relaxed);
}

void Application::update() {
	if (context.importRequested) {
		context.importRequested = false;
		showImportDialog();
	}

	if (audioManager)
		audioManager->update();

	engine->getAssetManager().flushPendingUploads();

	stepSimulation();
}

bool Application::handleLiveResize(void* userdata, SDL_Event* event) {
	if (event->type != SDL_EVENT_WINDOW_RESIZED &&
		event->type != SDL_EVENT_WINDOW_EXPOSED)
	{
		return true;
	}

	auto* self = static_cast<Application*>(userdata);

	if (SDL_GetWindowFromID(event->window.windowID) != self->window)
		return true;

	if (self->frameInProgress.load(std::memory_order::relaxed))
		return true;

	self->render();
	return true;
}

void Application::stepSimulation() {
	const float freq = static_cast<float>(SDL_GetPerformanceFrequency());
	const U64 now = SDL_GetPerformanceCounter();

	if (simulationState.lastPerfCounter == 0)
		simulationState.lastPerfCounter = now;

	float frameTime = static_cast<float>(now - simulationState.lastPerfCounter) / freq;
	simulationState.lastPerfCounter = now;

	if (frameTime > timingConfig.maxDeltaTime)
		frameTime = timingConfig.maxDeltaTime;

	if (!simulationState.playing) {
		simulationState.accumulated = 0.0f;
		simulationState.alpha = 1.0f;
		return;
	}

	simulationState.accumulated += frameTime;

	int numFixed = 0;
	float accumulatedCopy = simulationState.accumulated;

	while (
		accumulatedCopy >= timingConfig.fixedDeltaTime &&
		numFixed < timingConfig.maxFixedUpdates
	) {
		accumulatedCopy -= timingConfig.fixedDeltaTime;
		++numFixed;
	}

	simulationState.accumulated = (numFixed >= timingConfig.maxFixedUpdates)
		? 0.0f
		: accumulatedCopy;

	for (int i = 0; i < numFixed; ++i) {
		++simulationState.tick;
		context.activeWorld->fixedUpdate(timingConfig.fixedDeltaTime, simulationState.tick);
	}

	simulationState.alpha = simulationState.accumulated / timingConfig.fixedDeltaTime;
}

void Application::showImportDialog() {
	std::error_code ec;
	std::string defaultLocation = std::filesystem::absolute(context.assetsRoot, ec).string();

	SDL_ShowOpenFileDialog(
		&Application::handleImportDialog,
		this,
		window,
		importFilters.data(),
		static_cast<int>(importFilters.size()),
		defaultLocation.c_str(),
		true
	);
}

void Application::handleImportDialog(void* userdata, const char* const* filelist, int /*filter*/) {
	auto* self = static_cast<Application*>(userdata);

	if (!filelist) {
		BT_ERROR("Asset import: file dialog error - {}", SDL_GetError());
		return;
	}

	bool importedAny = false;

	for (const char* const* f = filelist; *f != nullptr; ++f) {
		std::filesystem::path source(*f);
		std::filesystem::path destination = self->context.assetsRoot / source.filename();

		std::error_code ec;
		std::filesystem::create_directories(self->context.assetsRoot, ec);

		std::filesystem::copy_file(
			source, destination,
			std::filesystem::copy_options::overwrite_existing,
			ec
		);

		if (ec) {
			BT_ERROR("Asset import: failed to copy '{}' to '{}': {}",
				source.string(), destination.string(), ec.message());
			continue;
		}

		importedAny = true;
	}

	if (importedAny)
		self->context.assetCache.markStale();
}

} // namespace Blackthorn::Editor