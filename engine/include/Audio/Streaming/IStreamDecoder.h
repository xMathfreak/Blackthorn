#pragma once

#include <filesystem>

#include "Audio/Resources/AudioData.h"
#include "Core/Export.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::Audio::Streaming {

/**
 * @brief Stateful, seekable PCM decoder interface for streaming audio.
 *
 * One @c IStreamDecoder instance lives per active streaming @c Voice,
 * owned by its @c StreamingVoiceState. It is opened once when the voice
 * starts and queried repeatedly by @c StreamingThread to fill successive
 * double-buffer slots. On loop, @c AudioThread calls @c seek(0) and
 * the decode pipeline continues from the start of the file.
 *
 * @section threading Thread safety
 * An @c IStreamDecoder is NOT thread-safe. While a @c StreamingJob is
 * in flight, the streaming thread has exclusive access to the decoder.
 * The audio thread must not call any method while a job referencing this
 * decoder is queued or executing. All seek calls are made on the audio
 * thread after the streaming thread has delivered its result command,
 * guaranteeing non-concurrent access by design.
 *
 * @section ownership Ownership
 * Owned by @c StreamingVoiceState. Destroyed when the voice is released
 * on the audio thread after its @c StreamingVoiceState is reset.
 */

class BLACKTHORN_API IStreamDecoder {
public:
	virtual ~IStreamDecoder() = default;

	/**
	 * @brief Opens the audio file and prepares the decoder for reading.
	 *
	 * Must be called exactly once before @c readFrames or @c seek.
	 *
	 * @param path Absolute path to the audio file.
	 * @return true on success.
	 */
	virtual bool open(const std::filesystem::path& path) = 0;

	/**
	 * @brief Opens an in-memory encoded audio buffer and prepares the
	 * decoder for reading.
	 *
	 * Alternative to open() for packed/in-memory assets that have no
	 * backing file on disk.
	 *
	 * @note The decoder does NOT take ownership of @p data. The caller
	 * (AudioClip, via its owned compressed-bytes buffer) must keep it alive
	 * for as long as this decoder remains open, i.e. for the lifetime of
	 * the streaming voice, matching the same "clip must outlive the voice"
	 * contract AudioManager::play() uses.
	 *
	 * @param data Pointer to the encoded audio bytes (the full compressed file).
	 * @param size Size of @p data in bytes.
	 * @return true on success.
	 */
	virtual bool openMemory(const U8* data, size_t size) = 0;

	/**
	 * @brief Reads up to @p frameCount interleaved PCM frames into @p dest.
	 *
	 * Samples are always 16-bit signed. The caller must ensure @p dest has
	 * room for at least @p frameCount * @c channels() * sizeof(I16) bytes.
	 *
	 * @param dest       Destination buffer.
	 * @param frameCount Maximum number of PCM frames to read.
	 * @return           Number of frames actually read. Returns 0 at EOF.
	 */
	virtual size_t readFrames(I16* dest, size_t frameCount) = 0;

	/**
	 * @brief Seeks to an absolute PCM frame offset.
	 *
	 * Used by the streaming manager to implement looping: when @c readFrames
	 * returns fewer frames than requested (EOF), the manager calls
	 * @c seek(0) and reads the remainder from the beginning of the file.
	 *
	 * @param frameOffset Absolute PCM frame position (0 = start of file).
	 * @return true on success.
	 */
	virtual bool seek(U64 frameOffset) = 0;

	/**
	 * @brief Closes the file handle and releases all decoder resources.
	 *
	 * Called implicitly by the destructor. Safe to call more than once.
	 */
	virtual void close() = 0;

	/** @brief Returns true if @c open() has been called successfully. */
	virtual bool isOpen() const = 0;

	/** @brief Returns the metadata of the open audio file. */
	virtual AudioMetadata info() const = 0;
};

} // namespace Blackthorn::Audio