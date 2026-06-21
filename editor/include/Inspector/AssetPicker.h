#pragma once

#include <typeindex>

#include <imgui.h>

#include "Assets/AssetDirectoryCache.h"
#include "Assets/AssetRegistry.h"
#include "Inspector/AssetPickerContext.h"

namespace Blackthorn::Editor::Inspector {

/**
 * @brief Draws a button + popup that lets the user reassign an
 * asset-typed component field (e.g. Sprite::texture) to any file under
 * the project's asset root matching @p T's registered extensions.
 *
 * Loading is synchronous, since it runs on the editor's main GL thread
 * in response to a direct user click - acceptable for typical asset
 * sizes and consistent with how this widget is invoked. Game code at
 * runtime should still prefer loadAsync as usual; this is editor-only.
 *
 * @tparam T Engine asset type. Must be registered via
 * Assets::AssetRegistry::registerAssetType<T>() during editor startup.
 */
template <typename T>
bool drawAssetPicker(const char* label, T*& assetPtr) {
	bool changed = false;

	auto* cache = AssetPickerContext::cache();
	auto* manager = AssetPickerContext::manager();

	const auto& registry = Assets::AssetRegistry::instance();
	const auto* typeEntry = registry.getEntry(std::type_index(typeid(T)));

	ImGui::Text("%s", label);
	ImGui::SameLine();

	if (ImGui::Button(assetPtr ? "(assigned)" : "(none)"))
		ImGui::OpenPopup(label);

	if (ImGui::BeginPopup(label)) {
		if (!typeEntry || !cache || !manager) {
			ImGui::TextDisabled("Asset picker unavailable");
		} else {
			for (const auto& entry : cache->entries()) {
				if (entry.assetType != std::type_index(typeid(T)))
					continue;

				std::string display = entry.relativePath.string();

				if (ImGui::Selectable(display.c_str())) {
					void* loaded = typeEntry->load(
						*manager,
						entry.relativePath.string(),
						entry.absolutePath.string()
					);

					if (loaded) {
						assetPtr = static_cast<T*>(loaded);
						changed = true;
					}
				}
			}
		}

		ImGui::EndPopup();
	}

	return changed;
}

} // namespace Blackthorn::Editor::Inspector