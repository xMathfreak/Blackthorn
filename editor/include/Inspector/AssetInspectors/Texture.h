#pragma once

#include <imgui.h>

#include "Assets/AssetEntry.h"
#include "Graphics/Texture.h"
#include "Inspector/AssetInspector.h"

namespace Blackthorn::Editor {

template <>
struct AssetInspector<Graphics::Texture> {
	static void draw(Graphics::Texture* texture, const Assets::AssetEntry& entry) {
		if (!texture) {
			ImGui::TextDisabled("Loading...");
			return;
		}

		ImGui::Text("%d x %d, %d channel(s)",
			texture->getWidth(), texture->getHeight(), texture->getChannels());

		float aspect = texture->getHeight() > 0
			? static_cast<float>(texture->getWidth()) / static_cast<float>(texture->getHeight())
			: 1.0f;

		float displayWidth = ImGui::GetContentRegionAvail().x - 1;
		ImGui::Image(
			(ImTextureID)(intptr_t)texture->getID(),
			{ displayWidth, displayWidth / aspect }
		);
	}
};

} // namespace Blackthorn::Editor