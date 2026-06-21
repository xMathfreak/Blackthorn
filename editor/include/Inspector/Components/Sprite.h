#pragma once

#include <imgui.h>

#include "ECS/Components/Sprite.h"
#include "Inspector/AssetPicker.h"
#include "Inspector/ComponentInspector.h"

namespace Blackthorn::Editor {

template <>
struct ComponentInspector<ECS::Components::Sprite> {
	static constexpr const char* name() {
		return "Sprite";
	}

	static bool draw(ECS::Components::Sprite& sprite) {
		bool changed = false;

		if (ImGui::CollapsingHeader("Sprite", ImGuiTreeNodeFlags_DefaultOpen)) {
			changed |= Inspector::drawAssetPicker<Graphics::Texture>("Texture", sprite.texture);

			changed |= ImGui::DragFloat2(
				"Dimensions",
				&sprite.width,
				0.1f
			);

			changed |= ImGui::DragFloat(
				"Z-Index",
				&sprite.zOrder,
				1.0f
			);
		}

		return changed;
	}
};

} // namespace Blackthorn::Editor