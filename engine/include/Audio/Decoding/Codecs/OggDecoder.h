#pragma once

#include <filesystem>

#include "Audio/Resources/AudioData.h"
#include "Core/Export.h"

namespace Blackthorn::Audio::Decoding {

class BLACKTHORN_API OggDecoder {
public:
	static bool decode(
		const std::filesystem::path& path,
		AudioData& data
	);

	static bool getInfo(
		const std::filesystem::path& path,
		AudioMetadata& data
	);
};

} // Blackthorn::Audio