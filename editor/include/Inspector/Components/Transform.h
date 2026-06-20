#pragma once

#include <imgui.h>

#include "ECS/Components/Transform.h"
#include "Inspector/ComponentInspector.h"

namespace Blackthorn::Editor {

template <>
struct ComponentInspector<ECS::Components::Transform> {
	static constexpr const char* name() {
		return "Transform";
	}

	static bool draw(ECS::Components::Transform& transform) {
		bool changed = false;

		if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
			changed |= ImGui::DragFloat2(
				"Position",
				&transform.position.x
			);

			changed |= ImGui::DragFloat(
				"Angle",
				&transform.angle,
				0.1f
			);

			changed |= ImGui::DragFloat(
				"Scale",
				&transform.scale,
				0.01f,
				0.0f,
				std::numeric_limits<float>::max(),
				"%.3f",
				ImGuiSliderFlags_AlwaysClamp
			);
		}

		return changed;
	}
};

} // namespace Blackthorn::Editor