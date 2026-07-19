#pragma once

#include <filesystem>

#include "Audio/Resources/AudioData.h"
#include "Core/Export.h"

namespace Blackthorn::Audio::Decoding {

class BLACKTHORN_API WavDecoder {
public:
	/**
	 * @brief Fully decodes a WAV file into interleaved I16 PCM samples.
	 *
	 * @param path Absolute path to the .wav file.
	 * @param data Receives the decoded samples.
	 * @return true on success.
	 */
	static bool decode(
		const std::filesystem::path& path,
		AudioData& data
	);

	/**
	 * @brief Reads only the metadata of a WAV file without decoding PCM.
	 *
	 * @param path Absolute path to the .wav file.
	 * @param data Receives the metadata (channels, sampleRate, frameCount).
	 * @return true on success.
	 */
	static bool getInfo(
		const std::filesystem::path& path,
		AudioMetadata& data
	);

	/**
	 * @brief Fully decodes WAV data from a memory buffer.
	 *
	 * Uses drwav_open_memory_and_read_pcm_frames_s16() so no temporary
	 * file is required. The buffer must remain valid for the duration
	 * of this call.
	 *
	 * @param src      Pointer to the encoded WAV bytes.
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
	 * @brief Reads only the metadata from WAV data in a memory buffer.
	 *
	 * Uses drwav_init_memory() to open a decoder without decoding samples,
	 * then reads totalPCMFrameCount / channels / sampleRate from the
	 * drwav struct.
	 *
	 * @param src      Pointer to the encoded WAV bytes.
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
