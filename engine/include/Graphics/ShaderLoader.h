#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "Assets/AssetManager.h"
#include "Assets/AssetResolver.h"
#include "Assets/IAssetLoader.h"
#include "Assets/LoadParams.h"
#include "Assets/RawAssetData.h"
#include "AssetNormalize.h"
#include "Core/Export.h"
#include "Debug/Logger.h"
#include "Graphics/Shader.h"

namespace Blackthorn::Graphics {

/**
 * @brief Load parameters for a shader program.
 *
 * The vertex shader is the asset's source path, so the program is addressed by
 * its vertex shader's ID. The fragment shader is a second canonical ID.
 *
 * @code
 * manager.load<Shader>("assets/shaders/sprite.vert", ShaderParams{
 *     "assets/shaders/sprite.vert", "assets/shaders/sprite.frag" });
 * @endcode
 */
struct BLACKTHORN_API ShaderParams final : Assets::AssetLoadParams {
	std::filesystem::path fragmentPath;

	ShaderParams(std::filesystem::path vertex, std::filesystem::path fragment)
		: Assets::AssetLoadParams(std::move(vertex))
		, fragmentPath(std::move(fragment))
	{}

	std::unique_ptr<Assets::LoadParams> clone() const override {
		return std::make_unique<ShaderParams>(*this);
	}
};

struct BLACKTHORN_API RawShaderData : Assets::IRawAssetData {
	std::string vertSource;
	std::string fragSource;

	RawShaderData() = default;
};

namespace Detail {

/**
 * @brief Canonical IDs for the vertex and fragment shaders in @p params.
 */
inline std::optional<std::pair<std::string, std::string>> shaderRequest(const Assets::LoadParams& params) {
	const auto* sp = dynamic_cast<const ShaderParams*>(&params);
	if (!sp) {
		BT_ERROR("ShaderLoader: expected ShaderParams");
		return std::nullopt;
	}

	const auto vertex = Blackthorn::normalizeAssetPath(sp->source.generic_string());
	const auto fragment = Blackthorn::normalizeAssetPath(sp->fragmentPath.generic_string());
	if (!vertex || !fragment) {
		BT_ERROR("ShaderLoader: invalid shader path (vertex '{}', fragment '{}')",
			sp->source.generic_string(), sp->fragmentPath.generic_string());
		return std::nullopt;
	}

	return std::make_pair(*vertex, *fragment);
}

/**
 * @brief Reads one shader source through the resolver as text.
 */
inline std::optional<std::string> readShaderSource(Assets::AssetResolver& resolver, const std::string& id) {
	const auto bytes = resolver.resolve(id);
	if (!bytes)
		return std::nullopt;

	return std::string(reinterpret_cast<const char*>(bytes->bytes.data()), bytes->bytes.size());
}

} // namespace Detail

/**
 * @brief Synchronous shader loader. Reads sources through the resolver and compiles them.
 */
class BLACKTHORN_API ShaderLoader final : public Assets::IAssetLoader<Shader> {
public:
	std::unique_ptr<Shader> load(const Assets::LoadParams& params) override {
		if (!resolver) {
			BT_ERROR("ShaderLoader: no resolver, loader was not registered");
			return nullptr;
		}

		const auto request = Detail::shaderRequest(params);
		if (!request)
			return nullptr;

		const auto vert = Detail::readShaderSource(*resolver, request->first);
		const auto frag = Detail::readShaderSource(*resolver, request->second);
		if (!vert || !frag)
			return nullptr;

		auto shader = std::make_unique<Shader>();
		if (!shader->compileFromSource(*vert, *frag)) {
			BT_ERROR("ShaderLoader: compilation failed for '{}'", request->first);
			return nullptr;
		}

		return shader;
	}
};

/**
 * @brief Asynchronous shader loader. Reads sources on a worker, compiles on the main thread.
 */
class BLACKTHORN_API AsyncShaderLoader final : public Assets::IAsyncAssetLoader<Shader> {
public:
	AsyncShaderLoader() = default;

	std::unique_ptr<Assets::IRawAssetData> loadRaw(const Assets::LoadParams& params) override {
		if (!resolver) {
			BT_ERROR("AsyncShaderLoader: no resolver, loader was not registered");
			return nullptr;
		}

		const auto request = Detail::shaderRequest(params);
		if (!request)
			return nullptr;

		const auto vert = Detail::readShaderSource(*resolver, request->first);
		const auto frag = Detail::readShaderSource(*resolver, request->second);
		if (!vert || !frag)
			return nullptr;

		auto raw = std::make_unique<RawShaderData>();
		raw->vertSource = *vert;
		raw->fragSource = *frag;
		raw->valid = true;
		return raw;
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
};

} // namespace Blackthorn::Graphics
