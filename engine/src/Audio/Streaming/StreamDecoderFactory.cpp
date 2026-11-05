#include "Audio/Streaming/StreamDecoderFactory.hpp"

#include <algorithm>

#include "Audio/Decoding/AudioDecoder.hpp"
#include "Audio/Streaming/Codecs/FlacStreamDecoder.hpp"
#include "Audio/Streaming/Codecs/Mp3StreamDecoder.hpp"
#include "Audio/Streaming/Codecs/OggStreamDecoder.hpp"
#include "Audio/Streaming/Codecs/WavStreamDecoder.hpp"
#include "Debug/Logger.hpp"

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

std::unique_ptr<IStreamDecoder> StreamDecoderFactory::createFromMemory(
	const U8* data,
	size_t size
) {
	const std::string ext = Decoding::AudioDecoder::detectFormat(data, size);

	if (ext == ".wav")
		return std::make_unique<WavStreamDecoder>();

	if (ext == ".mp3")
		return std::make_unique<Mp3StreamDecoder>();

	if (ext == ".flac")
		return std::make_unique<FlacStreamDecoder>();

	if (ext == ".ogg")
		return std::make_unique<OggStreamDecoder>();

	BT_ERROR(
		"StreamDecoderFactory: unrecognised in-memory format ({} bytes)",
		size
	);

	return nullptr;
}

bool StreamDecoderFactory::isSupported(const std::filesystem::path& path) noexcept {
	const std::string ext = toLower(path.extension().string());
	return ext == ".wav"
		|| ext == ".mp3"
		|| ext == ".flac"
		|| ext == ".ogg";
}

} // namespace Blackthorn::Audio