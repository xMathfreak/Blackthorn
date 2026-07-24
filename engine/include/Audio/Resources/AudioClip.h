#pragma once

#include <optional>
#include <filesystem>
#include <vector>

#include "Audio/Decoding/AudioDecoder.h"
#include "Audio/Resources/AudioData.h"
#include "Core/Export.h"
#include "Core/Types/Numeric.h"

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
		bool ok = false;

		if (compressedBytes) {
			ok = Decoding::AudioDecoder::decodeFromMemory(
				compressedBytes->data(), compressedBytes->size(), decoded
			);

			if (!ok) {
				BT_ERROR(
					"AudioClip: loadPCM() failed to decode packed clip ({} bytes)",
					compressedBytes->size()
				);
			}
		} else {
			ok = Decoding::AudioDecoder::decode(clipPath, decoded);

			if (!ok) {
				BT_ERROR(
					"AudioClip: loadPCM() failed to decode clip '{}'",
					clipPath.string()
				);
			}
		}

		if (!ok)
			return false;

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
		compressedBytes.reset();
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

	/**
	 * @brief True if this clip was loaded from a pack and retains its
	 * compressed source bytes.
	 *
	 * When true, loadPCM() and AudioThread's streaming path decode from
	 * compressedData()/compressedSize() instead of sourcePath(), a packed
	 * asset generally has no file at sourcePath() that exists at runtime.
	 */
	[[nodiscard]]
	bool hasCompressedSource() const noexcept {
		return compressedBytes.has_value();
	}

	/// Pointer to the owned compressed source bytes, or nullptr if this clip
	/// wasn't loaded from a pack (see hasCompressedSource()).
	[[nodiscard]]
	const U8* compressedData() const noexcept {
		return compressedBytes ? compressedBytes->data() : nullptr;
	}

	/// Size in bytes of compressedData(), or 0 if hasCompressedSource() is false.
	[[nodiscard]]
	size_t compressedSize() const noexcept {
		return compressedBytes ? compressedBytes->size() : 0;
	}

	[[nodiscard]]
	const AudioData& data() const noexcept {
		return *pcmData;
	}

	void loadFromMemory(
		std::filesystem::path path,
		AudioMetadata& meta,
		std::optional<AudioData> pcm,
		std::optional<std::vector<U8>> sourceBytes = std::nullopt
	) {
		unload();

		clipPath = std::move(path);
		metaData = std::move(meta);

		pcmData = std::move(pcm);
		compressedBytes = std::move(sourceBytes);

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

	/// Owned compressed source bytes for packed loads.
	std::optional<std::vector<U8>> compressedBytes;

	double duration = 0.0;
	bool loaded = false;
};

} // namespace Blackthorn::Audio