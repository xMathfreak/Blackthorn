#pragma once

#include "Core/Export.h"
#include <string>

namespace Blackthorn::Assets {

struct BLACKTHORN_API IRawAssetData {
	virtual ~IRawAssetData() = default;

	std::string assetID;

	bool valid = false;

protected:
	IRawAssetData() = default;
	explicit IRawAssetData(std::string id)
		: assetID(std::move(id))
	{}
};

} //namespace Blackthorn::Assets