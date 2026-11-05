#pragma once

#include <string>

#include "Blackthorn/Core/Export.hpp"
#include "Blackthorn/Types/Numeric.hpp"

namespace Blackthorn::Assets {

struct BLACKTHORN_API IRawAssetData {
	virtual ~IRawAssetData() = default;

	U32 assetName = U32_MAX;
	std::string displayName;
	bool valid = false;

protected:
	IRawAssetData() = default;

	explicit IRawAssetData(U32 name)
		: assetName(name)
	{}
};

} //namespace Blackthorn::Assets