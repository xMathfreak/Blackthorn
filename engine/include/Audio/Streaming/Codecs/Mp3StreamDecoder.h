#pragma once

#include <filesystem>

#include "Audio/Resources/AudioData.h"
#include "Audio/Streaming/IStreamDecoder.h"
#include "Core/Export.h"

namespace Blackthorn::Audio::Streaming {

class BLACKTHORN_API Mp3StreamDecoder final : public IStreamDecoder {
public:
	Mp3StreamDecoder();
	~Mp3StreamDecoder() override;

	Mp3StreamDecoder(const Mp3StreamDecoder&) = delete;
	Mp3StreamDecoder& operator=(const Mp3StreamDecoder&) = delete;

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