#pragma once

#include "Audio/AudioHandle.h"
#include "Audio/Streaming/IStreamDecoder.h"
#include "Containers/SPSCRingQueue.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::Audio {

/**
 * @brief Descriptor for one decode chunk, pushed by the audio thread and
 *        consumed by @c StreamingThread.
 *
 * @c StreamingJob is a lightweight value type that lives in the SPSC ring
 * queue without heap allocation. The @c decoder pointer is non-owning —
 * the decoder is owned by @c StreamingVoiceState, which is owned by the
 * @c Voice, which remains active until the audio thread processes the
 * corresponding @c StreamBufferReadyCommand. This guarantees the decoder
 * outlives the job.
 *
 * @section looping Loop hint
 * @c looping controls how the streaming thread reports EOF. When true and
 * @c readFrames returns fewer frames than requested, the thread still pushes
 * a @c StreamBufferReadyCommand for the partial chunk but sets
 * @c endOfStream = false. The audio thread then calls
 * @c IStreamDecoder::seek(0) on the decoder (safe because the streaming
 * thread has finished with it) and submits a new job for the remainder.
 * This keeps all seek calls on the audio thread.
 */
struct StreamingJob {
	/// Handle of the voice this job belongs to. Routes the resulting
	/// @c StreamBufferReadyCommand back to the correct voice.
	AudioHandle handle;

	/// Non-owning pointer to the voice's decoder. Owned by
	/// @c StreamingVoiceState. Must not be null.
	Streaming::IStreamDecoder* decoder = nullptr;

	/// Number of PCM frames to decode in this job.
	size_t frameCount = 0;

	/// Channel count, cached from the stream metadata to avoid a virtual
	/// call per job.
	U32 channels = 0;

	/// Sample rate in Hz, passed through to @c StreamBufferReadyCommand.
	U32 sampleRate = 0;

	/// If true, a short read sets @c endOfStream = false in the result
	/// command (the audio thread handles seeking for loop continuation).
	/// If false, a short read sets @c endOfStream = true.
	bool looping = false;
};

using StreamingJobQueue =
	Containers::SPSCRingQueue<StreamingJob, 128>;

} // namespace Blackthorn::Audio