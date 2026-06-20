#include "Editor.h"

#include <glad/glad.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>

#include "Graphics/GLLoader.h"

#include "Inspector/InspectorRegistry.h"
#include "Inspector/Components/Transform.h"
#include "Inspector/Components/Sprite.h"
#include "Inspector/Components/Kinematics.h"
#include "Inspector/Components/Persistent.h"

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
		640,
		480,
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

		if (area->y < self->titleBarState.height && !self->titleBarState.itemHovered)
			return SDL_HITTEST_DRAGGABLE;

		return SDL_HITTEST_NORMAL;
	};

	SDL_SetWindowHitTest(window, hitTest, this);

	auto& inspector = Inspector::InspectorRegistry::instance();

	inspector.registerInspectorItem<ECS::Components::Transform>();
	inspector.registerInspectorItem<ECS::Components::Sprite>();
	inspector.registerInspectorItem<ECS::Components::Kinematics>();
	inspector.registerInspectorItem<ECS::Components::Persistent>();


	context.activeWorld = &world;
	initialized = true;
	return true;
}

void Application::shutdown() {
	if (!initialized)
		return;

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
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	titleBar.draw(window, running, titleBarState, context);
	dockspace.draw(titleBarState, dockspaceState, running);
	hierarchy.draw(context);
	inspector.draw(context);
	viewport.draw(context, *renderer, viewportState);

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
}

void Application::update() {

}

} // namespace Blackthorn