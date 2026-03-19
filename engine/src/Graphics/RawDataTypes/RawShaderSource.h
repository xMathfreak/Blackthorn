#pragma once

#include "Assets/RawAssetData.h"
#include "Core/Export.h"

namespace Blackthorn::Graphics {

struct BLACKTHORN_API RawShaderSource : Assets::IRawAssetData {
	std::string		vertexSource;
	std::string		fragmentSource;

	// Retained for error messages during main-thread compilation.
	std::string		vertexPath;
	std::string		fragmentPath;

	RawShaderSource() = default;
	explicit RawShaderSource(std::string id) : IRawAssetData(std::move(id)) {}
};

} // namespace Blackthorn::Graphics
