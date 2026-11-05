#pragma once

#include <filesystem>

#include "Audio/Resources/AudioData.h"
#include "Core/Export.h"

namespace Blackthorn::Audio::Decoding {

class BLACKTHORN_API OggDecoder {
public:
	/**
	 * @brief Fully decodes an OGG Vorbis file into interleaved I16 PCM samples.
	 *
	 * @param path Absolute path to the .ogg file.
	 * @param data Receives the decoded samples and is sized to hold the entire stream.
	 * @return true on success.
	 */
	static bool decode(
		const std::filesystem::path& path,
		AudioData& data
	);

	/**
	 * @brief Reads only the metadata of an OGG Vorbis file without decoding PCM.
	 *
	 * @param path Absolute path to the .ogg file.
	 * @param data Receives the metadata (channels, sampleRate, frameCount).
	 * @return true on success.
	 */
	static bool getInfo(
		const std::filesystem::path& path,
		AudioMetadata& data
	);

	/**
	 * @brief Fully decodes OGG Vorbis data from a memory buffer.
	 *
	 * Uses ov_open_callbacks() with a cursor-backed ov_callbacks struct so
	 * no temporary file is required. The buffer must remain valid for the
	 * lifetime of this call.
	 *
	 * @param src      Pointer to the encoded OGG bytes.
	 * @param srcSize  Byte size of the buffer.
	 * @param data     Receives the decoded samples.
	 * @return true on success.
	 */
	static bool decodeFromMemory(
		const U8* src,
		size_t srcSize,
		AudioData& data
	);

	/**
	 * @brief Reads only the metadata from OGG Vorbis data in a memory buffer.
	 *
	 * @param src      Pointer to the encoded OGG bytes.
	 * @param srcSize  Byte size of the buffer.
	 * @param data     Receives the metadata (channels, sampleRate, frameCount).
	 * @return true on success.
	 */
	static bool getInfoFromMemory(
		const U8* src,
		size_t srcSize,
		AudioMetadata& data
	);
};

} // namespace Blackthorn::Audio::Decoding
