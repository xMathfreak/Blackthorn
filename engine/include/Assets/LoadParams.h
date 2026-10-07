#pragma once

#include <filesystem>
#include <memory>

#include "Core/Export.h"

namespace Blackthorn::Assets {

struct BLACKTHORN_API LoadParams {
	virtual ~LoadParams() = default;
	virtual std::unique_ptr<LoadParams> clone() const = 0;
};

struct BLACKTHORN_API AssetLoadParams : LoadParams {
	std::filesystem::path source; ///< Asset file path either as a loose file or resolved through AssetResolver.

#ifdef BT_PACK_MODE
	std::filesystem::path packPath; ///< If set, only search this specific pack.
#endif

	explicit AssetLoadParams(std::filesystem::path p)
		: source(std::move(p))
	{}

	std::unique_ptr<LoadParams> clone() const override {
		return std::make_unique<AssetLoadParams>(*this);
	}
};

} // namespace Blackthorn::Assets