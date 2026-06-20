#include "Panels/Viewport.h"

#include <imgui.h>

#include "ECS/World.h"
#include "Graphics/Renderer.h"

namespace Blackthorn::Editor::Panels {

void Viewport::draw(
	State::Context& context,
	Graphics::Renderer& renderer,
	State::Viewport& viewport,
	float alpha
) {
	ImGui::Begin("Viewport");

	ImVec2 avail = ImGui::GetContentRegionAvail();

	if (avail.x <= 0.0f || avail.y <= 0) {
		ImGui::End();
		return;
	}

	U32 width = static_cast<U32>(avail.x);
	U32 height = static_cast<U32>(avail.y);

	if (
		width > 0 &&
		height > 0 &&
		(width != viewport.width || height != viewport.height)
	) {
		viewport.width = width;
		viewport.height = height;
		renderer.setProjection(width, height);
	}

	renderer.beginScene();

	if (context.activeWorld)
		context.activeWorld->render(alpha);

	renderer.endScene();

	const Graphics::Texture& texture = renderer.getSceneTexture();

	ImGui::Image(
		(ImTextureID)(intptr_t)texture.getID(),
		avail,
		{0, 1},
		{1, 0}
	);

	ImGui::End();
}

} // namespace Blackthorn::Editor::Panels