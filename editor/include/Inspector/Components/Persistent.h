#pragma once

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "ECS/Components/Persistent.h"
#include "Inspector/ComponentInspector.h"

namespace Blackthorn::Editor {

template <>
struct ComponentInspector<ECS::Components::Persistent> {
	static constexpr const char* name() {
		return "Persistent";
	}

	static bool draw(ECS::Components::Persistent& pers) {
		bool changed = false;

		if (ImGui::CollapsingHeader("Persistent", ImGuiTreeNodeFlags_DefaultOpen)) {
			changed |= ImGui::InputText(
				"Name",
				&pers.name
			);
		}

		return changed;
	}
};

} // namespace Blackthorn::Editor