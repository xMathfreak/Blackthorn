#include "Panels/Inspector.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "Inspector/InspectorRegistry.h"
#include "ECS/Components/Tag.h"
#include "ECS/World.h"

namespace Blackthorn::Editor::Panels {

void Inspector::draw(
	State::Context& context
) {
	ImGui::Begin("Inspector");

	if (
		context.selectedEntity == ECS::INVALID_ENTITY ||
		!context.activeWorld->isValid(context.selectedEntity)
	) {
		context.selectedEntity = ECS::INVALID_ENTITY;
		ImGui::End();
		return;
	}

	auto* tag = context.activeWorld->getComponent<ECS::Components::Tag>(context.selectedEntity);

	if (tag) {
		ImGui::InputText("Name", &tag->name);
		ImGui::Separator();
	} else {
		ImGui::TextDisabled("(no Tag component)");
	}

	auto& reg = ::Blackthorn::Editor::Inspector::InspectorRegistry::instance();
	auto& pool = context.activeWorld->getPool();

	for (size_t i = 0; i < ECS::Detail::MAX_COMPONENTS; ++i) {
		const auto* regEntry = reg.getEntry(i);
		if (!regEntry)
			continue;

		void* comp = pool.getComponentRaw(context.selectedEntity, i);

		if (comp) {
			ImGui::PushID(static_cast<int>(i));
			regEntry->draw(comp);

			ImGui::SameLine();

			if (ImGui::SmallButton("X"))
				regEntry->destroy(pool, context.selectedEntity);

			ImGui::PopID();
		}
	}

	if (ImGui::Button("Add Component"))
		ImGui::OpenPopup("AddComponentPopup");

	if (ImGui::BeginPopup("AddComponentPopup")) {
		for (size_t i = 0; i < ECS::Detail::MAX_COMPONENTS; ++i) {
			const auto* regEntry = reg.getEntry(i);
			if (!regEntry || !regEntry->construct)
				continue;

			void* existing = pool.getComponentRaw(context.selectedEntity, i);
			if (existing)
				continue;

			if (ImGui::MenuItem(regEntry->name.data()))
				regEntry->construct(pool, context.selectedEntity);
		}

		ImGui::EndPopup();
	}

	ImGui::End();
}

} // namespace Blackthorn::Editor::Panels