#pragma once

#include <filesystem>

#include "Audio/Resources/AudioData.h"
#include "Audio/Streaming/IStreamDecoder.h"
#include "Core/Export.h"

namespace Blackthorn::Audio::Streaming {

class BLACKTHORN_API OggStreamDecoder final : public IStreamDecoder {
public:
	OggStreamDecoder();
	~OggStreamDecoder() override;

	OggStreamDecoder(const OggStreamDecoder&) = delete;
	OggStreamDecoder& operator=(const OggStreamDecoder&) = delete;

	bool open(const std::filesystem::path& path) override;
	size_t readFrames(I16* dest, size_t frameCount) override;
	bool seek(U64 frameOffset) override;
	void close() override;

	AudioMetadata info() const override;
	bool isOpen() const override;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace Blackthorn::Audio