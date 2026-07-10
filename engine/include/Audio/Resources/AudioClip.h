#pragma once

#include <optional>
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

		metaData = info;
		clipPath = path;

		if (metaData.sampleRate > 0 && metaData.frameCount > 0) {
			duration = static_cast<double>(metaData.frameCount) /
						static_cast<double>(metaData.sampleRate);
		}

		loaded = true;
		return true;
	}

	bool loadPCM() {
		if (!loaded) {
			BT_WARN("AudioClip: loadPCM() called before load()");
			return false;
		}

		AudioData decoded;
		if (!Decoding::AudioDecoder::decode(clipPath, decoded)) {
			BT_ERROR(
				"AudioClip: loadPCM() failed to decode clip '{}'",
				clipPath.string()
			);
			return false;
		}

		pcmData = std::move(decoded);
		return true;
	}

	void clearPCM() noexcept {
		pcmData.reset();
	}

	void unload() noexcept {
		clipPath = std::filesystem::path("");
		metaData = {};
		pcmData.reset();
		duration = 0.0;
		loaded = false;
	}

	bool isLoaded() const noexcept {
		return loaded;
	}

	[[nodiscard]]
	const AudioMetadata& metadata() const noexcept {
		return metaData;
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
		return metaData.frameCount;
	}

	[[nodiscard]]
	U32 sampleRate() const noexcept {
		return metaData.sampleRate;
	}

	[[nodiscard]]
	U32 channels() const noexcept {
		return metaData.channels;
	}

	[[nodiscard]]
	size_t estimatedBytes() const noexcept {
		return metaData.frameCount * metaData.channels * sizeof(I16);
	}

	[[nodiscard]]
	bool hasPCM() const noexcept {
		return pcmData.has_value();
	}

	[[nodiscard]]
	const AudioData& data() const noexcept {
		return *pcmData;
	}

	void loadFromMemory(std::filesystem::path path, AudioMetadata& meta, std::optional<AudioData> pcm) {
		unload();

		clipPath = path;
		metaData = std::move(meta);

		pcmData = std::move(pcm);

		if (metaData.sampleRate > 0 && metaData.frameCount > 0) {
			duration = static_cast<double>(metaData.frameCount) /
						static_cast<double>(metaData.sampleRate);
		}

		loaded = true;
	}

private:
	std::filesystem::path clipPath;
	AudioMetadata metaData;
	std::optional<AudioData> pcmData;
	double duration = 0.0;
	bool loaded = false;
};

} // namespace Blackthorn::Audio