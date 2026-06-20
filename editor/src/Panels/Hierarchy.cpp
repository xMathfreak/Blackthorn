#include "Panels/Hierarchy.h"

#include <imgui.h>

#include "ECS/Components/Tag.h"
#include "ECS/World.h"

namespace Blackthorn::Editor::Panels {

void Hierarchy::draw(
	State::Context& context
) {
	ImGui::Begin("Hierarchy");

	if (context.activeWorld) {
		if (ImGui::Button("Create Entity")) {
			ECS::Entity entity = context.activeWorld->createEntity();
			context.selectedEntity = entity;
		}

		const auto& pool = context.activeWorld->getPool();
		const auto& entityData = pool.getEntities();

		for (U32 idx = 0; idx < static_cast<U32>(entityData.size()); ++idx) {
			const auto& ed = entityData[idx];

			if (!ed.alive)
				continue;

			ECS::Entity e = ECS::Detail::makeEntity(idx, ed.generation);

			auto* tag = context.activeWorld->getComponent<ECS::Components::Tag>(e);
			std::string name = (tag && tag->name.length() > 0)
				? tag->name : ("Entity " + std::to_string(idx + 1));

			bool selected = e == context.selectedEntity;

			if (ImGui::Selectable(name.c_str(), selected)) {
				context.selectedEntity = e;
			}

			if (ImGui::BeginPopupContextItem()) {
				if (ImGui::MenuItem("Delete")) {
					context.activeWorld->destroyEntity(e);

					if (context.selectedEntity == e)
						context.selectedEntity = ECS::INVALID_ENTITY;
				}

				ImGui::EndPopup();
			}
		}
	}

	ImGui::End();
}

} // namespace Blackthorn::Editor::Panels