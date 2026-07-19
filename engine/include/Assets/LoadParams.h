#pragma once

#include <filesystem>
#include <memory>

#include "Core/Export.h"

namespace Blackthorn::Assets {

struct BLACKTHORN_API LoadParams {
	virtual ~LoadParams() = default;
	virtual std::unique_ptr<LoadParams> clone() const = 0;
};

struct BLACKTHORN_API PathLoadParams final : LoadParams {
	std::filesystem::path path;

	PathLoadParams(std::filesystem::path p)
		: path(std::move(p))
	{}

	std::unique_ptr<LoadParams> clone() const override {
		return std::make_unique<PathLoadParams>(*this);
	}
};

/**
 * @brief Explicitly loads an asset from a named pack entry.
 *
 * Use when you need to bypass the default resolver priority and
 * load from a specific pack by name (e.g. always load the base
 * game version of an asset, ignoring mods).
 */
struct BLACKTHORN_API PackLoadParams final : LoadParams {
	std::string assetID; ///< String ID to resolve through the AssetResolver.
	std::filesystem::path packPath; ///< If set, only search this specific pack.

	explicit PackLoadParams(std::string id)
		: assetID(std::move(id))
	{}

	PackLoadParams(std::string id, std::filesystem::path pack)
		: assetID(std::move(id))
		, packPath(std::move(pack))
	{}

	std::unique_ptr<LoadParams> clone() const override {
		return std::make_unique<PackLoadParams>(*this);
	}
};

} // namespace Blackthorn::Assets