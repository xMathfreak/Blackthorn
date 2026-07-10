#include "Audio/Streaming/StreamDecoderFactory.h"

#include <algorithm>

#include "Audio/Streaming/Codecs/FlacStreamDecoder.h"
#include "Audio/Streaming/Codecs/Mp3StreamDecoder.h"
#include "Audio/Streaming/Codecs/OggStreamDecoder.h"
#include "Audio/Streaming/Codecs/WavStreamDecoder.h"
#include "Debug/Logger.h"

namespace Blackthorn::Audio::Streaming {

namespace {

std::string toLower(std::string str) {
	std::transform(str.begin(), str.end(), str.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); }
	);
	return str;
}

} // anonymous namespace

std::unique_ptr<IStreamDecoder> StreamDecoderFactory::create(
	const std::filesystem::path& path
) {
	const auto ext = toLower(path.extension().string());

	if (ext == ".wav")
		return std::make_unique<WavStreamDecoder>();

	if (ext == ".mp3")
		return std::make_unique<Mp3StreamDecoder>();

	if (ext == ".flac")
		return std::make_unique<FlacStreamDecoder>();

	if (ext == ".ogg")
		return std::make_unique<OggStreamDecoder>();

	BT_ERROR(
		"StreamDecoderFactory: unsupported extension '{}' for path '{}'",
		ext, path.string()
	);
	return nullptr;
}

bool StreamDecoderFactory::isSupported(const std::filesystem::path& path) noexcept {
	const std::string ext = toLower(path.string());
	return ext == ".wav"
		|| ext == ".mp3"
		|| ext == ".flac"
		|| ext == ".ogg";
}

} // namespace Blackthorn::Audio