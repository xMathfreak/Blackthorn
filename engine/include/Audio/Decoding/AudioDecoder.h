#pragma once

#include <algorithm>
#include <filesystem>

#include "Audio/Resources/AudioData.h"
#include "Core/Export.h"
#include "Debug/Logger.h"

#include "Audio/Decoding/Codecs/OggDecoder.h"
#include "Audio/Decoding/Codecs/Mp3Decoder.h"
#include "Audio/Decoding/Codecs/FlacDecoder.h"
#include "Audio/Decoding/Codecs/WavDecoder.h"

namespace Blackthorn::Audio::Decoding {

namespace {

	std::string getExtension(const std::filesystem::path& path) {
		std::string ext = path.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(),
			[](unsigned char c) { return std::tolower(c); }
		);
		return ext;
	}

}

class BLACKTHORN_API AudioDecoder {
public:
	/**
	 * @brief Fully decodes @p path into @p data.
	 *
	 * Allocates @p data.samples to hold the entire decoded PCM stream as
	 * I16 interleaved samples. Only appropriate for resident clips; for
	 * streaming clips use @c getInfo() + @c IStreamDecoder.
	 *
	 * @param path Absolute path to the audio file.
	 * @param data Receives the decoded samples and metadata.
	 * @return     true on success.
	 */
	static bool decode(
		const std::filesystem::path& path,
		AudioData& data
	) {
		std::string ext = getExtension(path);

		if (ext == ".ogg")
			return OggDecoder::decode(path, data);

		if (ext == ".wav")
			return WavDecoder::decode(path, data);

		if (ext == ".flac")
			return FlacDecoder::decode(path, data);

		if (ext == ".mp3")
			return Mp3Decoder::decode(path, data);

		BT_ERROR("AudioDecoder: Unsupported extension '{}'", ext);
		return false;
	}

	/**
	 * @brief Reads only the metadata of @p path without decoding PCM.
	 *
	 * Used by @c StreamingAudioClip::load() to populate channel count,
	 * sample rate, and frame count without opening a persistent decoder.
	 * Much cheaper than @c decode() for long files.
	 *
	 * @param path Absolute path to the audio file.
	 * @param info Receives the metadata on success.
	 * @return     true on success.
	 */
	static bool getInfo(
		const std::filesystem::path& path,
		AudioMetadata& data
	) {
		std::string ext = getExtension(path);

		if (ext == ".ogg")
			return OggDecoder::getInfo(path, data);

		if (ext == ".wav")
			return WavDecoder::getInfo(path, data);

		if (ext == ".flac")
			return FlacDecoder::getInfo(path, data);

		if (ext == ".mp3")
			return Mp3Decoder::getInfo(path, data);

		BT_ERROR("AudioDecoder: Unsupported extension '{}'", ext);
		return false;
	}
};

} // Blackthorn::Audio