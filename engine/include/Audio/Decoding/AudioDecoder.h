#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>

#include "Audio/Resources/AudioData.h"
#include "Core/Export.h"
#include "Debug/Logger.h"

#include "Audio/Decoding/Codecs/FlacDecoder.h"
#include "Audio/Decoding/Codecs/Mp3Decoder.h"
#include "Audio/Decoding/Codecs/OggDecoder.h"
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

	/**
	 * @brief Detects the audio container format from the first few bytes.
	 *
	 * Used in the memory-backed decode/getInfo paths where no file extension
	 * is available. Returns a lowercase extension string (".ogg", ".mp3",
	 * ".flac", ".wav") or an empty string if the format is unrecognised.
	 *
	 * Detection logic:
	 *   OGG:  bytes[0..3] == "OggS"
	 *   FLAC: bytes[0..3] == "fLaC"
	 *   WAV:  bytes[0..3] == "RIFF"  &&  bytes[8..11] == "WAVE"
	 *   MP3:  ID3 tag    (bytes[0..2] == "ID3")
	 *          or sync word (bytes[0] == 0xFF && (bytes[1] & 0xE0) == 0xE0)
	 */
	std::string detectFormat(const uint8_t* src, size_t srcSize) {
		if (srcSize < 12)
			return {};

		// OGG Vorbis / OPUS container.
		if (src[0] == 'O' && src[1] == 'g' && src[2] == 'g' && src[3] == 'S')
			return ".ogg";

		// FLAC native stream marker.
		if (src[0] == 'f' && src[1] == 'L' && src[2] == 'a' && src[3] == 'C')
			return ".flac";

		// RIFF/WAVE container.
		if (src[0] == 'R' && src[1] == 'I' && src[2] == 'F' && src[3] == 'F' &&
		    src[8] == 'W' && src[9] == 'A' && src[10] == 'V' && src[11] == 'E')
			return ".wav";

		// MP3: ID3v2 tag header.
		if (src[0] == 'I' && src[1] == 'D' && src[2] == '3')
			return ".mp3";

		// MP3: raw sync word (no ID3 tag).
		if (src[0] == 0xFF && (src[1] & 0xE0) == 0xE0)
			return ".mp3";

		return {};
	}

} // anonymous namespace

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
		const std::string ext = getExtension(path);

		if (ext == ".ogg")
			return OggDecoder::decode(path, data);

		if (ext == ".wav")
			return WavDecoder::decode(path, data);

		if (ext == ".flac")
			return FlacDecoder::decode(path, data);

		if (ext == ".mp3")
			return Mp3Decoder::decode(path, data);

		BT_ERROR("AudioDecoder::decode: unsupported extension '{}' for '{}'", ext, path.string());
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
	 * @param data Receives the metadata on success.
	 * @return     true on success.
	 */
	static bool getInfo(
		const std::filesystem::path& path,
		AudioMetadata& data
	) {
		const std::string ext = getExtension(path);

		if (ext == ".ogg")
			return OggDecoder::getInfo(path, data);

		if (ext == ".wav")
			return WavDecoder::getInfo(path, data);

		if (ext == ".flac")
			return FlacDecoder::getInfo(path, data);

		if (ext == ".mp3")
			return Mp3Decoder::getInfo(path, data);

		BT_ERROR("AudioDecoder::getInfo: unsupported extension '{}' for '{}'", ext, path.string());
		return false;
	}

	/**
	 * @brief Fully decodes audio data from a memory buffer into @p data.
	 *
	 * The audio container format is detected automatically from the first
	 * bytes of the buffer via magic-number inspection, so no file extension
	 * or filename is required. The buffer must remain valid for the duration
	 * of this call.
	 *
	 * Supported formats: OGG Vorbis, MP3, FLAC, WAV.
	 *
	 * @param src      Pointer to the encoded audio bytes.
	 * @param srcSize  Byte size of the buffer.
	 * @param data     Receives the decoded I16 interleaved PCM samples.
	 * @return         true on success; false if the format is unrecognised
	 *                 or the codec reports an error.
	 */
	static bool decodeFromMemory(
		const uint8_t* src,
		size_t         srcSize,
		AudioData&     data
	) {
		const std::string ext = detectFormat(src, srcSize);

		if (ext == ".ogg")
			return OggDecoder::decodeFromMemory(src, srcSize, data);

		if (ext == ".wav")
			return WavDecoder::decodeFromMemory(src, srcSize, data);

		if (ext == ".flac")
			return FlacDecoder::decodeFromMemory(src, srcSize, data);

		if (ext == ".mp3")
			return Mp3Decoder::decodeFromMemory(src, srcSize, data);

		BT_ERROR("AudioDecoder::decodeFromMemory: unrecognised format (magic-number detection failed)");
		return false;
	}

	/**
	 * @brief Reads only the metadata from a memory buffer without decoding PCM.
	 *
	 * The audio container format is detected automatically from the first
	 * bytes of the buffer via magic-number inspection. The buffer must remain
	 * valid for the duration of this call.
	 *
	 * Supported formats: OGG Vorbis, MP3, FLAC, WAV.
	 *
	 * @param src      Pointer to the encoded audio bytes.
	 * @param srcSize  Byte size of the buffer.
	 * @param data     Receives the metadata (channels, sampleRate, frameCount).
	 * @return         true on success; false if the format is unrecognised
	 *                 or the codec reports an error.
	 */
	static bool getInfoFromMemory(
		const uint8_t* src,
		size_t         srcSize,
		AudioMetadata& data
	) {
		const std::string ext = detectFormat(src, srcSize);

		if (ext == ".ogg")
			return OggDecoder::getInfoFromMemory(src, srcSize, data);

		if (ext == ".wav")
			return WavDecoder::getInfoFromMemory(src, srcSize, data);

		if (ext == ".flac")
			return FlacDecoder::getInfoFromMemory(src, srcSize, data);

		if (ext == ".mp3")
			return Mp3Decoder::getInfoFromMemory(src, srcSize, data);

		BT_ERROR("AudioDecoder::getInfoFromMemory: unrecognised format (magic-number detection failed)");
		return false;
	}
};

} // namespace Blackthorn::Audio::Decoding
