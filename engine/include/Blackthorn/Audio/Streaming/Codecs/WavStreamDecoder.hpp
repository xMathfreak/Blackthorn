#pragma once

#include <filesystem>
#include <memory>

#include "Blackthorn/Assets/AssetStream.hpp"
#include "Blackthorn/Audio/Resources/AudioData.hpp"
#include "Blackthorn/Audio/Streaming/IStreamDecoder.hpp"
#include "Blackthorn/Core/Export.hpp"

namespace Blackthorn::Audio::Streaming {

class BLACKTHORN_API WavStreamDecoder final : public IStreamDecoder {
public:
	WavStreamDecoder();
	~WavStreamDecoder() override;

	WavStreamDecoder(const WavStreamDecoder&) = delete;
	WavStreamDecoder& operator=(const WavStreamDecoder&) = delete;

	bool open(const std::filesystem::path& path) override;
	bool openMemory(const U8* data, size_t size) override;
	bool openStream(std::unique_ptr<Assets::AssetStream> stream) override;
	size_t readFrames(I16* dest, size_t frameCount) override;
	bool seek(U64 frameOffset) override;
	void close() override;

	[[nodiscard]] AudioMetadata info() const override;
	[[nodiscard]] bool isOpen() const override;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

} // namespace Blackthorn::Audio