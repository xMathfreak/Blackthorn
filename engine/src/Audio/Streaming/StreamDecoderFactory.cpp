#include "Blackthorn/Audio/Streaming/StreamDecoderFactory.hpp"

#include <algorithm>

#include "Blackthorn/Audio/Decoding/AudioDecoder.hpp"
#include "Blackthorn/Audio/Streaming/Codecs/FlacStreamDecoder.hpp"
#include "Blackthorn/Audio/Streaming/Codecs/Mp3StreamDecoder.hpp"
#include "Blackthorn/Audio/Streaming/Codecs/OggStreamDecoder.hpp"
#include "Blackthorn/Audio/Streaming/Codecs/WavStreamDecoder.hpp"
#include "Blackthorn/Debug/Logger.hpp"
#include "StringHelper.hpp"

namespace Blackthorn::Audio::Streaming {

std::unique_ptr<IStreamDecoder> StreamDecoderFactory::create(
	const std::filesystem::path& path
) {
	const auto ext = String::toLower(path.extension().string());

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
	const std::string ext = String::toLower(path.extension().string());
	return ext == ".wav"
		|| ext == ".mp3"
		|| ext == ".flac"
		|| ext == ".ogg";
}

} // namespace Blackthorn::Audio