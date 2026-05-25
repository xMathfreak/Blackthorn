#pragma once

#include <filesystem>

#include "Audio/Resources/AudioData.h"
#include "Core/Export.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::Audio::Streaming {

/**
 * @brief Stateful, seekable PCM decoder interface for streaming audio.
 *
 * One @c IStreamDecoder instance lives per active streaming @c VoiceSlot.
 * It is opened once when the voice starts, then queried repeatedly by the
 * streaming worker to fill successive ring-buffer segments. On loop the
 * audio manager calls @c seek(0) and continues reading.
 *
 * @section threading Thread safety
 * An @c IStreamDecoder is NOT thread-safe. It is owned by a
 * @c std::shared_ptr that is captured into the decode lambda and therefore
 * lives on a @c ThreadPool worker thread for the duration of one decode job.
 * The audio thread must not call any method while @c decodingInFlight is set
 * on the owning @c VoiceSlot.
 *
 * @section ownership Ownership
 * The @c VoiceSlot holds the @c shared_ptr<IStreamDecoder>. When
 * @c releaseSlot is called the slot's pointer is moved into the pending
 * lambda, so the decoder is destroyed on the worker thread after the job
 * completes rather than on the audio thread. This avoids blocking the audio
 * thread on file-handle teardown.
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