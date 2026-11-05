#pragma once

#include "Blackthorn/Audio/Commands/AudioCommand.hpp"
#include "Blackthorn/Containers/SPSCRingQueue.hpp"

namespace Blackthorn::Audio {

using AudioCommandQueue =
	Containers::SPSCRingQueue<AudioCommand, 1024>;

/**
 * @brief SPSC queue used by @c StreamingThread to push decoded-chunk results
 *        back to the audio thread.
 */
using StreamDecodedQueue =
	Containers::SPSCRingQueue<AudioCommand, 128>;

} // namespace Blackthorn::Audio