#include "Panels/AssetBrowser.h"

#include <imgui.h>

#include "Assets/AssetRegistry.h"
#include "Assets/AssetManager.h"
#include "Graphics/Texture.h"

namespace Blackthorn::Editor::Panels {

namespace {

constexpr float kThumbnailSize = 24.0f;

void drawFileRow(
	const Editor::Assets::AssetEntry& entry,
	const Editor::Assets::AssetRegistry& registry,
	Blackthorn::Assets::AssetManager& manager,
	State::Context& context
) {
	const auto* typeEntry = registry.getEntry(entry.assetType);
	std::string filename = entry.relativePath.filename().string();

	ImGui::PushID(entry.relativePath.string().c_str());

	if (!typeEntry) {
		ImGui::TextDisabled("%s", filename.c_str());
		ImGui::PopID();
		return;
	}

	if (entry.assetType == std::type_index(typeid(Graphics::Texture))) {
		if (!entry.cachedPtr) {
			entry.cachedPtr = typeEntry->load(
				manager, entry.relativePath.string(), entry.absolutePath
			);
		}

		if (entry.cachedPtr) {
			auto* tex = static_cast<Graphics::Texture*>(entry.cachedPtr);
			ImGui::Image((ImTextureID)(intptr_t)tex->getID(), { kThumbnailSize, kThumbnailSize });
		} else {
			ImGui::Dummy({ kThumbnailSize, kThumbnailSize });
		}
	} else {
		ImGui::TextDisabled("[%s]", typeEntry->name.data());
	}

	ImGui::SameLine();

	bool selected = context.selectedAsset.has_value()
		&& context.selectedAsset->relativePath == entry.relativePath;

	if (ImGui::Selectable(filename.c_str()))
		context.selectedAsset = entry;

	if (ImGui::BeginDragDropSource()) {
		std::string payload = entry.relativePath.string();
		ImGui::SetDragDropPayload("BT_ASSET_PATH", payload.c_str(), payload.size() + 1);
		ImGui::Text("%s", payload.c_str());
		ImGui::EndDragDropSource();
	}

	ImGui::PopID();
}

void drawTreeNode(
	const Editor::Assets::AssetTreeNode& node,
	const Editor::Assets::AssetRegistry& registry,
	Blackthorn::Assets::AssetManager& manager,
	State::Context& context
) {
	for (const auto& dir : node.directories) {
		ImGui::PushID(dir.name.c_str());

		if (ImGui::TreeNode(dir.name.c_str())) {
			drawTreeNode(dir, registry, manager, context);
			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	const auto cache = context.assetCache;
	for (size_t idx : node.fileIndices)
		drawFileRow(cache.entries()[idx], registry, manager, context);
}

} // namespace

void AssetBrowser::draw(State::Context& context, Blackthorn::Assets::AssetManager& manager) {
	ImGui::Begin("Assets");

	if (context.assetCache.isStale())
		context.assetCache.refresh(context.assetsRoot);

	if (ImGui::Button("Refresh"))
		context.assetCache.refresh(context.assetsRoot);

	ImGui::SameLine();
	if (ImGui::Button("Import..."))
		context.importRequested = true;

	ImGui::SameLine();
	ImGui::TextDisabled("%s", context.assetsRoot.string().c_str());
	ImGui::Separator();

	const auto& registry = Blackthorn::Editor::Assets::AssetRegistry::instance();
	drawTreeNode(context.assetCache.tree(), registry, manager, context);

	ImGui::End();
}

} // namespace Blackthorn::Editor::Panels