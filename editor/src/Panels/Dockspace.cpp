#include "Panels/Dockspace.h"

#include "State/Dockspace.h"
#include "State/TitleBar.h"

#include <imgui.h>

namespace Blackthorn::Editor::Panels {

void Dockspace::draw(
	State::Titlebar& titleBar,
	State::Dockspace& dockspace,
	State::Context& context,
	bool& running
) {
	ImGuiWindowFlags windowFlags =
		ImGuiWindowFlags_MenuBar |
		ImGuiWindowFlags_NoDocking;

	const ImGuiViewport* vp = ImGui::GetMainViewport();

	ImGui::SetNextWindowPos({
		vp->Pos.x,
		vp->Pos.y + titleBar.height
	});

	ImGui::SetNextWindowSize({
		vp->Size.x,
		vp->Size.y - titleBar.height
	});

	ImGui::SetNextWindowViewport(vp->ID);

	windowFlags |=
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin("MainDockspace", &dockspace.open, windowFlags);

	ImGui::PopStyleVar(3);

	ImGuiID dockspaceID = ImGui::GetID("MainDockspace");

	ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Exit"))
				running = false;

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit")) {
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Options")) {
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Assets")) {
			if (ImGui::MenuItem("Refresh"))
				context.assetCache.markStale();

			if (ImGui::MenuItem("Import..."))
				context.importRequested = true;

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Tools")) {
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Windows")) {
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}

	ImGui::End();
}

} // namespace Blackthorn::Editor::Panels