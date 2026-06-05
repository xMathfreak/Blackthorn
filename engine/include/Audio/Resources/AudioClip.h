#pragma once

#include <filesystem>

#include "Audio/Decoding/AudioDecoder.h"
#include "Audio/Resources/AudioData.h"
#include "Core/Export.h"

namespace Blackthorn::Audio {

class BLACKTHORN_API AudioClip {
public:
	AudioClip() = default;
	AudioClip(const std::filesystem::path& path) {
		load(path);
	}

	AudioClip(const AudioClip&) = delete;
	AudioClip& operator=(const AudioClip&) = delete;

	AudioClip(AudioClip&&) = default;
	AudioClip& operator=(AudioClip&&) = default;

	bool load(const std::filesystem::path& path) {
		unload();

		AudioMetadata info;
		if (!Decoding::AudioDecoder::getInfo(path, info)) {
			BT_ERROR("AudioClip: Failed to read metadata from {}", path.string());
			return false;
		}

		data = info;
		clipPath = path;

		if (data.sampleRate > 0 && data.frameCount > 0) {
			duration = static_cast<double>(data.frameCount) /
						static_cast<double>(data.sampleRate);
		}

		loaded = true;
		return true;
	}

	void unload() noexcept {
		clipPath = std::filesystem::path("");
		data = {};
		duration = 0.0;
		loaded = false;
	}

	bool isLoaded() const noexcept {
		return loaded;
	}

	[[nodiscard]]
	const AudioMetadata& metadata() const noexcept {
		return data;
	}

	[[nodiscard]]
	double durationSeconds() const noexcept {
		return duration;
	}

	[[nodiscard]]
	const std::filesystem::path& sourcePath() const noexcept {
		return clipPath;
	}

	[[nodiscard]]
	U64 frameCount() const noexcept {
		return data.frameCount;
	}

	[[nodiscard]]
	U32 sampleRate() const noexcept {
		return data.sampleRate;
	}

	[[nodiscard]]
	U32 channels() const noexcept {
		return data.channels;
	}

	[[nodiscard]]
	size_t estimatedBytes() const noexcept {
		return data.frameCount * data.channels * sizeof(float);
	}

private:
	std::filesystem::path clipPath;
	AudioMetadata data;
	double duration = 0.0;
	bool loaded = false;
};

} // namespace Blackthorn::Audio