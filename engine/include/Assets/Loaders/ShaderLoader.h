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

class BLACKTHORN_API ShaderLoader final : public Assets::IAssetLoader<Shader> {
public:
	std::unique_ptr<Shader> load(const Assets::LoadParams& params) override {
		if (auto* p = dynamic_cast<const ShaderParams*>(&params))
			return std::make_unique<Shader>(p->vertexPath, p->fragmentPath);

		return nullptr;

	}

	std::vector<std::string> getSupportedExtensions() const override {
		return {".glsl", ".frag", ".vert"};
	}
};

} // namespace Blackthorn::Graphics