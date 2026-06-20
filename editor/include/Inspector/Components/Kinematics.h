#pragma once

#include <imgui.h>

#include "ECS/Components/Kinematics.h"
#include "Inspector/ComponentInspector.h"

namespace Blackthorn::Editor {

template <>
struct ComponentInspector<ECS::Components::Kinematics> {
	static constexpr const char* name() {
		return "Kinematics";
	}

	static bool draw(ECS::Components::Kinematics& km) {
		bool changed = false;

		if (ImGui::CollapsingHeader("Kinematics", ImGuiTreeNodeFlags_DefaultOpen)) {
			changed |= ImGui::DragFloat2(
				"Old Position",
				&km.oldPosition.x
			);

			changed |= ImGui::DragFloat2(
				"Acceleration",
				&km.acceleration.x
			);
		}

		return changed;
	}
};

} // namespace Blackthorn::Editor