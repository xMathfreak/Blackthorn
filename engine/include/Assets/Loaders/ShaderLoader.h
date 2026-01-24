#pragma once

#include "Assets/IAssetLoader.h"
#include "Graphics/Shader.h"

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
	};
};

class ShaderLoader : public Assets::IAssetLoader<Shader> {
public:
	std::unique_ptr<Shader> load(const Assets::LoadParams& params) override {
		const auto& p  = static_cast<const ShaderParams&>(params);
		std::unique_ptr<Shader> shader = std::make_unique<Shader>(p.vertexPath, p.fragmentPath);
		return shader;
	}

	std::vector<std::string> getSupportedExtensions() const override {
		return {".glsl", ".frag", ".vert"};
	}
};

} // namespace Blackthorn::Graphics