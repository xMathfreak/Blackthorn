#pragma once

#include <string>
#include <vector>

#include "Assets/AssetManager.h"
#include "Assets/IAssetLoader.h"
#include "Assets/LoadParams.h"
#include "Assets/RawAssetData.h"
#include "Core/Export.h"
#include "Debug/Logger.h"
#include "Graphics/Shader.h"

#ifdef BT_PACK_MODE
	#include "Assets/AssetResolver.h"
	#include "Assets/PackMount.h"
#endif

namespace Blackthorn::Graphics {

struct BLACKTHORN_API ShaderParams : Assets::LoadParams {
	std::string vertexPath;
	std::string fragmentPath;

	ShaderParams(const std::string& vertex, const std::string& fragment)
		: vertexPath(vertex)
		, fragmentPath(fragment)
	{}

	std::unique_ptr<Assets::LoadParams> clone() const override {
		return std::make_unique<ShaderParams>(*this);
	}
};

struct BLACKTHORN_API PackShaderParams final : Assets::LoadParams {
	std::string vertID;
	std::string fragID;

	PackShaderParams(std::string vert, std::string frag)
		: vertID(std::move(vert))
		, fragID(std::move(frag))
	{}

	std::unique_ptr<Assets::LoadParams> clone() const override {
		return std::make_unique<PackShaderParams>(*this);
	}
};

struct BLACKTHORN_API RawShaderData : Assets::IRawAssetData {
	std::string vertSource;
	std::string fragSource;

	RawShaderData() = default;
};

class BLACKTHORN_API ShaderLoader final : public Assets::IAssetLoader<Shader> {
public:
	std::unique_ptr<Shader> load(const Assets::LoadParams& params) override {
		if (const auto* p = dynamic_cast<const ShaderParams*>(&params))
			return std::make_unique<Shader>(p->vertexPath, p->fragmentPath);

		return nullptr;
	}

	std::vector<std::string> getSupportedExtensions() const override {
		return { ".glsl", ".frag", ".vert" };
	}
};

class BLACKTHORN_API AsyncShaderLoader final : public Assets::IAsyncAssetLoader<Shader> {
public:
#ifdef BT_PACK_MODE
	explicit AsyncShaderLoader(Assets::AssetResolver* resolver)
		: m_resolver(resolver)
	{}
#else
	AsyncShaderLoader() = default;
#endif

	std::unique_ptr<Assets::IRawAssetData> loadRaw(const Assets::LoadParams& params) override {
#ifdef BT_PACK_MODE
		return loadRawFromPack(params);
#else
		return loadRawFromDisk(params);
#endif
	}

	void upload(Assets::IRawAssetData& rawBase, Assets::AssetManager& manager) override {
		auto& raw = static_cast<RawShaderData&>(rawBase);

		auto shader = std::make_unique<Shader>();
		if (!shader->compileFromSource(raw.vertSource, raw.fragSource)) {
			BT_ERROR("AsyncShaderLoader: shader compilation failed for '{}'", raw.assetID);
			return;
		}

		manager.add<Shader>(raw.assetID, std::move(shader));
		BT_DEBUG("AsyncShaderLoader: '{}' compiled and linked", raw.assetID);
	}

	std::vector<std::string> getSupportedExtensions() const override {
		return { ".glsl", ".frag", ".vert" };
	}

private:

#ifdef BT_PACK_MODE
	std::unique_ptr<Assets::IRawAssetData> loadRawFromPack(const Assets::LoadParams& params) {
		const auto* pp = dynamic_cast<const PackShaderParams*>(&params);
		if (!pp) {
			BT_ERROR("AsyncShaderLoader: BT_PACK_MODE requires PackShaderParams "
				"(vertID + fragID). Got a different LoadParams type.");
			return nullptr;
		}

		if (!m_resolver) {
			BT_ERROR("AsyncShaderLoader: resolver is null, was registerPackLoader() used?");
			return nullptr;
		}

		auto vertData = m_resolver->resolve(pp->vertID);
		if (!vertData) {
			BT_ERROR("AsyncShaderLoader: vertex shader '{}' not found in any mounted pack",
				pp->vertID);
			return nullptr;
		}

		auto fragData = m_resolver->resolve(pp->fragID);
		if (!fragData) {
			BT_ERROR("AsyncShaderLoader: fragment shader '{}' not found in any mounted pack",
				pp->fragID);
			return nullptr;
		}

		auto raw = std::make_unique<RawShaderData>();

		raw->vertSource.assign(
			reinterpret_cast<const char*>(vertData->bytes.data()),
			vertData->bytes.size()
		);
		raw->fragSource.assign(
			reinterpret_cast<const char*>(fragData->bytes.data()),
			fragData->bytes.size()
		);

		raw->valid = true;
		return raw;
	}

	Assets::AssetResolver* m_resolver = nullptr;
#endif

	std::unique_ptr<Assets::IRawAssetData> loadRawFromDisk(const Assets::LoadParams& params) {
		const ShaderParams* sp = dynamic_cast<const ShaderParams*>(&params);
		if (!sp) {
			BT_ERROR("AsyncShaderLoader: expected ShaderParams in debug mode");
			return nullptr;
		}

		auto raw = std::make_unique<RawShaderData>();

		if (!readTextFile(sp->vertexPath, raw->vertSource)) {
			BT_ERROR("AsyncShaderLoader: failed to read vertex shader '{}'", sp->vertexPath);
			return nullptr;
		}

		if (!readTextFile(sp->fragmentPath, raw->fragSource)) {
			BT_ERROR("AsyncShaderLoader: failed to read fragment shader '{}'", sp->fragmentPath);
			return nullptr;
		}

		raw->valid = true;
		return raw;
	}

	static bool readTextFile(const std::string& path, std::string& out) {
		std::FILE* f = std::fopen(path.c_str(), "rb");
		if (!f)
			return false;

		std::fseek(f, 0, SEEK_END);
		const long size = std::ftell(f);
		std::fseek(f, 0, SEEK_SET);

		if (size < 0) {
			std::fclose(f);
			return false;
		}

		out.resize(static_cast<size_t>(size));
		const bool ok = std::fread(out.data(), 1, out.size(), f) == out.size();
		std::fclose(f);
		return ok;
	}
};

} // namespace Blackthorn::Graphics
