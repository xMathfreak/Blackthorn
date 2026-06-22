#pragma once

#include <cstdlib>
#include <filesystem>
#include <string>

#include <imgui.h>

#include "Assets/AssetEntry.h"
#include "Graphics/Shader.h"
#include "Inspector/AssetInspector.h"

namespace Blackthorn::Editor {

inline void openInExternalEditor(const std::filesystem::path& path) {
	std::string command;

#if defined(_WIN32)
	command = "start \"\" \"" + path.string() + "\"";
#elif defined(__APPLE__)
	command = "open \"" + path.string() + "\"";
#elif defined(__linux__)
	command = "xdg-open \"" + path.string() + "\"";
#endif

	if (!command.empty())
		std::system(command.c_str());
}

template <>
struct AssetInspector<Graphics::Shader> {
	static void draw(Graphics::Shader* /*shader*/, const Assets::AssetEntry& entry) {
		ImGui::Text("Source: %s", entry.absolutePath.string().c_str());

		if (ImGui::Button("Open in External Editor"))
			openInExternalEditor(entry.absolutePath);

		ImGui::TextDisabled(
			"Shaders are compiled at load time.\n"
			"Re-import and restart to see external edits."
		);
	}
};

} // namespace Blackthorn::Editor