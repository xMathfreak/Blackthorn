#pragma once

#include <filesystem>

#include "Audio/Resources/AudioData.h"
#include "Core/Export.h"

namespace Blackthorn::Audio::Decoding {

class BLACKTHORN_API Mp3Decoder {
public:
	/**
	 * @brief Fully decodes an MP3 file into interleaved I16 PCM samples.
	 *
	 * @param path Absolute path to the .mp3 file.
	 * @param data Receives the decoded samples.
	 * @return true on success.
	 */
	static bool decode(
		const std::filesystem::path& path,
		AudioData& data
	);

	/**
	 * @brief Reads only the metadata of an MP3 file without decoding PCM.
	 *
	 * @param path Absolute path to the .mp3 file.
	 * @param data Receives the metadata (channels, sampleRate, frameCount).
	 * @return true on success.
	 */
	static bool getInfo(
		const std::filesystem::path& path,
		AudioMetadata& data
	);

	/**
	 * @brief Fully decodes MP3 data from a memory buffer.
	 *
	 * Uses drmp3_open_memory_and_read_pcm_frames_s16() so no temporary
	 * file is required. The buffer must remain valid for the duration
	 * of this call.
	 *
	 * @param src      Pointer to the encoded MP3 bytes.
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
	 * @brief Reads only the metadata from MP3 data in a memory buffer.
	 *
	 * Uses drmp3_init_memory() to open a decoder without decoding samples,
	 * then drmp3_get_pcm_frame_count() for the frame count.
	 *
	 * @param src      Pointer to the encoded MP3 bytes.
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
