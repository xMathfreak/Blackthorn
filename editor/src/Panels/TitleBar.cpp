#include "Panels/TitleBar.h"

#include <imgui.h>

#include "Scene/IClientScene.h"

namespace Blackthorn::Editor::Panels {

namespace {

using IconDrawFn = void(*)(const ImVec2& center, float size, ImU32 color);

void drawMinimizeIcon(const ImVec2& center, float size, ImU32 color) {
	ImDrawList* dl = ImGui::GetWindowDrawList();
	float half = size * 0.5f;

	dl->AddLine(
		{ center.x - half, center.y },
		{ center.x + half, center.y },
		color, 1.5f
	);
}

void drawMaximizeIcon(const ImVec2& center, float size, ImU32 color) {
	ImDrawList* dl = ImGui::GetWindowDrawList();
	float half = size * 0.5f;

	dl->AddRect(
		{ center.x - half, center.y - half },
		{ center.x + half, center.y + half },
		color, 0.0f, 0, 1.5f
	);
}

void drawRestoreIcon(const ImVec2& center, float size, ImU32 color) {
	ImDrawList* dl = ImGui::GetWindowDrawList();
	float half = size * 0.5f;
	float offset = size * 0.18f;

	// Back square, offset up-right.
	dl->AddRect(
		{ center.x - half + offset, center.y - half - offset },
		{ center.x + half + offset, center.y + half - offset },
		color, 0.0f, 0, 1.5f
	);

	// Front square. Filled background first to mask the overlap with the
	// back square, matching the standard OS "restore" glyph convention.
	ImU32 bg = ImGui::GetColorU32(ImGuiCol_WindowBg);

	dl->AddRectFilled(
		{ center.x - half - offset, center.y - half + offset },
		{ center.x + half - offset, center.y + half + offset },
		bg
	);

	dl->AddRect(
		{ center.x - half - offset, center.y - half + offset },
		{ center.x + half - offset, center.y + half + offset },
		color, 0.0f, 0, 1.5f
	);
}

void drawCloseIcon(const ImVec2& center, float size, ImU32 color) {
	ImDrawList* dl = ImGui::GetWindowDrawList();
	float half = size * 0.5f;

	dl->AddLine(
		{ center.x - half, center.y - half },
		{ center.x + half, center.y + half },
		color, 1.5f
	);

	dl->AddLine(
		{ center.x - half, center.y + half },
		{ center.x + half, center.y - half },
		color, 1.5f
	);
}

/**
 * @brief Draws a title-bar button with a vector-drawn icon centered inside
 * it, instead of a text glyph.
 *
 * Reuses ImGui::Button for hit-testing and the existing hover/active color
 * styling, then overlays the icon via ImDrawList so it isn't subject to
 * font glyph coverage or atlas rasterization size.
 */
bool titleBarButton(const char* id, const ImVec2& size, IconDrawFn drawIcon) {
	bool clicked = ImGui::Button(id, size);

	ImVec2 min = ImGui::GetItemRectMin();
	ImVec2 max = ImGui::GetItemRectMax();
	ImVec2 center{ (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f };

	const float iconSize = 10.0f;
	const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);

	drawIcon(center, iconSize, color);

	return clicked;
}

} // namespace

void TitleBar::draw(
	SDL_Window* window,
	bool& running,
	State::Titlebar& titleBar,
	State::Context& context
) {
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));

	ImGuiViewport* vp = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(vp->Pos);
	ImGui::SetNextWindowSize({vp->Size.x, titleBar.height});

	ImGui::Begin(
		"##TitleBar",
		nullptr,
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoSavedSettings
	);

	const float lineHeight = ImGui::GetTextLineHeight();
	ImGui::SetCursorPos({8.0f, (titleBar.height - lineHeight) * 0.5f});
	ImGui::Text("Blackthorn");
	ImGui::SameLine();

	std::string projectName =
		context.projectName.empty()
			? "Untitled Project"
			: context.projectName;

	std::string sceneName = "Untitled Scene";

	if (context.activeScene) {
		const char* name = context.activeScene->getName();

		if (name && *name != '\0')
			sceneName = name;
	}

	std::string title = projectName + " - " + sceneName;
	ImGui::TextDisabled("| %s", title.c_str());

	float x = ImGui::GetWindowWidth() - titleBar.buttonWidth * 3;

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.153f, 0.153f, 0.153f, 1.0f));

	ImGui::SetCursorPos({x, 0.0f});
	if (titleBarButton("##minimize", {titleBar.buttonWidth, titleBar.height}, drawMinimizeIcon))
		SDL_MinimizeWindow(window);

	x += titleBar.buttonWidth;
	bool maximized = SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED;
	ImGui::SetCursorPos({x, 0.0f});
	if (titleBarButton(
		"##maximize",
		{titleBar.buttonWidth, titleBar.height},
		maximized ? drawRestoreIcon : drawMaximizeIcon
	)) {
		if (maximized) SDL_RestoreWindow(window);
		else SDL_MaximizeWindow(window);
	}

	ImGui::PopStyleColor(1); // pop the grey hover used by minimize/maximize

	x += titleBar.buttonWidth;
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.91f, 0.067f, 0.137f, 1.0f));
	ImGui::SetCursorPos({x, 0.0f});
	if (titleBarButton("##close", {titleBar.buttonWidth, titleBar.height}, drawCloseIcon))
		running = false;

	ImGui::PopStyleColor(3);

	titleBar.itemHovered = ImGui::IsAnyItemHovered() ||
		!ImGui::IsWindowHovered();

	ImGui::PopStyleVar(3);
	ImGui::End();
}

} // namespace Blackthorn::Editor::Panels