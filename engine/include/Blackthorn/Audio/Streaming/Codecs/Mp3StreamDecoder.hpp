#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "Blackthorn/Assets/AssetStream.hpp"

#include "Blackthorn/Audio/Resources/AudioData.hpp"
#include "Blackthorn/Audio/Streaming/IStreamDecoder.hpp"
#include "Blackthorn/Core/Export.hpp"

namespace Blackthorn::Audio::Streaming {

class BLACKTHORN_API Mp3StreamDecoder final : public IStreamDecoder {
public:
	Mp3StreamDecoder();
	~Mp3StreamDecoder() override;

	Mp3StreamDecoder(const Mp3StreamDecoder&) = delete;
	Mp3StreamDecoder& operator=(const Mp3StreamDecoder&) = delete;

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

	/// Shared by open()/openMemory() after drmp3_init_file()/init_memory()
	/// succeeds: builds the seek table and reads metadata. sourceLabel is
	/// used in log messages only.
	bool finalizeAfterInit(const std::string& sourceLabel);
};

} // namespace Blackthorn::Audio