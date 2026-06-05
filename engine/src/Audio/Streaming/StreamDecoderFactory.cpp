#include "Audio/Streaming/StreamDecoderFactory.h"

#include <algorithm>
#include <filesystem>

#include "Audio/Streaming/Codecs/FlacStreamDecoder.h"
#include "Audio/Streaming/Codecs/Mp3StreamDecoder.h"
#include "Audio/Streaming/Codecs/OggStreamDecoder.h"
#include "Audio/Streaming/Codecs/WavStreamDecoder.h"
#include "Debug/Logger.h"

namespace Blackthorn::Audio::Streaming {

namespace {

/// Returns the lowercase extension of @p path including the leading dot.
std::string lowerExtension(const std::string& path) {
	std::string ext = std::filesystem::path(path).extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return ext;
}

} // anonymous namespace

std::unique_ptr<IStreamDecoder> StreamDecoderFactory::create(
	const std::string& path
) {
	const std::string ext = lowerExtension(path);

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
		ext, path
	);
	return nullptr;
}

bool StreamDecoderFactory::isSupported(const std::string& path) noexcept {
	const std::string ext = lowerExtension(path);
	return ext == ".wav"
		|| ext == ".mp3"
		|| ext == ".flac"
		|| ext == ".ogg";
}

} // namespace Blackthorn::Audio