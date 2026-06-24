#pragma once

#include <filesystem>

#include <imgui.h>

#include "Assets/AssetEntry.h"
#include "Audio/AudioManager.h"
#include "Audio/Resources/AudioClip.h"
#include "Inspector/AssetInspector.h"
#include "Inspector/AudioPreviewContext.h"

namespace Blackthorn::Editor {

template <>
struct AssetInspector<Audio::AudioClip> {
	static void draw(Audio::AudioClip* clip, const Assets::AssetEntry& entry) {
		if (!clip) {
			ImGui::TextDisabled("Loading...");
			return;
		}

		static std::filesystem::path lastPath;
		static Audio::AudioHandle handle;

		auto* manager = AudioPreviewContext::manager();

		if (entry.relativePath != lastPath) {
			if (manager && handle.isValid())
				manager->stop(handle);

			handle = Audio::AudioHandle::invalid();
			lastPath = entry.relativePath;
		}

		ImGui::Text("%.2fs, %u Hz, %u ch", clip->durationSeconds(), clip->sampleRate(), clip->channels());

		if (!manager) {
			ImGui::TextDisabled("Audio preview unavailable");
			return;
		}

		const bool playing = handle.isValid() && manager->isPlaying(handle);
		const bool paused = handle.isValid() && manager->isPaused(handle);
		const bool stopped = !playing && !paused;

		if (stopped) {
			if (ImGui::Button("Play")) {
				Audio::PlayOptions opts;
				opts.mode = Audio::PlaybackMode::Stream;
				handle = manager->play(*clip, opts);
			}
		} else if (playing) {
			if (ImGui::Button("Pause"))
				manager->pause(handle);
		} else {
			if (ImGui::Button("Resume"))
				manager->resume(handle);
		}

		ImGui::SameLine();
		if (ImGui::Button("Stop") && handle.isValid())
			manager->stop(handle);

		if (handle.isValid()) {
			float t = manager->getPlaybackTime(handle);
			float dur = manager->getDuration(handle);

			if (ImGui::SliderFloat("##seek", &t, 0.0f, dur > 0.0f ? dur : 1.0f, "%.2fs")) {
				manager->seek(handle, t);
			}
		}
	}
};

} // namespace Blackthorn::Editor