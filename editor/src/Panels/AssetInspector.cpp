#include "Panels/AssetInspector.h"

#include <imgui.h>

#include "Assets/AssetRegistry.h"

namespace Blackthorn::Editor::Panels {

void AssetInspector::draw(State::Context& context, Blackthorn::Assets::AssetManager& manager) {
	ImGui::Begin("Asset Inspector");
	const auto& registry = Blackthorn::Editor::Assets::AssetRegistry::instance();

	if (!context.selectedAsset) {
		if (lastEntry) {
			const auto* prev = registry.getEntry(lastEntry->assetType);
			if (prev && prev->load)
				manager.unloadById(lastEntry->relativePath.string());
		}

		lastEntry.reset();
		ImGui::TextDisabled("No asset selected");
		ImGui::End();
		return;
	}

	const auto& entry = *context.selectedAsset;

	if (!lastEntry || lastEntry->relativePath != entry.relativePath) {
		if (lastEntry) {
			const auto* prev = registry.getEntry(lastEntry->assetType);

			if (prev && prev->load)
				manager.unloadById(lastEntry->relativePath.string());
		}

		lastEntry = entry;
	}

	const auto* typeEntry = registry.getEntry(lastEntry->assetType);
	ImGui::Text("%s", entry.relativePath.string().c_str());

	if (!typeEntry) {
		ImGui::TextDisabled("Unrecognized asset type");
		ImGui::End();
		return;
	}

	ImGui::TextDisabled("%s", typeEntry->name.data());
	ImGui::Separator();

	void* loaded = nullptr;

	if (typeEntry->load)
		loaded = typeEntry->load(manager, entry.relativePath.string(), entry.absolutePath);

	if (typeEntry->drawInspector) {
		typeEntry->drawInspector(loaded, entry);
	} else {
		ImGui::TextDisabled("No inspector registered for this type");
	}

	ImGui::End();
}

} // namespace Blackthorn::Editor::Panels