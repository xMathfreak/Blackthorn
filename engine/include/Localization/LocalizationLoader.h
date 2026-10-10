#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Assets/AssetManager.h"
#include "Assets/AssetResolver.h"
#include "Assets/IAssetLoader.h"
#include "Assets/LoadParams.h"
#include "Assets/RawAssetData.h"
#include "AssetNormalize.h"
#include "Core/Export.h"
#include "Debug/Logger.h"
#include "Localization/LocalizationManager.h"
#include "Localization/LocalizationSource.h"

namespace Blackthorn::Localization {

/**
 * @brief Non-owning proxy registered with AssetManager for a locale source
 * owned by LocalizationManager.
 *
 * Destroying the proxy unloads the source. It's non-copyable so that a copy
 * can't unload the source twice.
 */
class BLACKTHORN_API LocalizationSource {
public:
	explicit LocalizationSource(SourceHandle h, LocalizationManager& lm)
		: locMan(lm)
		, handle(h)
	{}

	~LocalizationSource() {
		locMan.unloadSource(handle);
	}

	LocalizationSource(const LocalizationSource&) = delete;
	LocalizationSource& operator=(const LocalizationSource&) = delete;

	[[nodiscard]] SourceHandle getHandle() const noexcept { return handle; }

private:
	LocalizationManager& locMan;
	SourceHandle handle;
};

/**
 * @brief Load parameters for a locale source.
 *
 * The file is the asset's source path. A `.btloc` extension selects the
 * compiled format. Any other extension is treated as JSON text.
 */
struct BLACKTHORN_API LocaleLoadParams final : Assets::AssetLoadParams {
	I32 priority = 0;

	LocaleLoadParams(std::filesystem::path p, I32 pr = 0)
		: Assets::AssetLoadParams(std::move(p))
		, priority(pr)
	{}

	std::unique_ptr<Assets::LoadParams> clone() const override {
		return std::make_unique<LocaleLoadParams>(*this);
	}
};

/// Raw bytes for one locale source, plus the priority it should load with.
struct BLACKTHORN_API RawLocalizationSourceData : Assets::IRawAssetData {
	std::vector<U8> bytes;
	I32 priority = 0;
	bool compiled = false;   ///< True for `.btloc`, false for JSON.
};

namespace Detail {

struct LocaleRequest {
	std::string id;
	I32         priority = 0;
	bool        compiled = false;
};

/**
 * @brief Canonical ID, priority, and format for a locale request.
 */
inline std::optional<LocaleRequest> localeRequest(const Assets::LoadParams& params) {
	const auto* lp = dynamic_cast<const LocaleLoadParams*>(&params);
	if (!lp) {
		BT_ERROR("LocalizationSourceLoader: expected LocaleLoadParams");
		return std::nullopt;
	}

	const auto id = Blackthorn::normalizeAssetPath(lp->source.generic_string());
	if (!id) {
		BT_ERROR("LocalizationSourceLoader: invalid asset path '{}'", lp->source.generic_string());
		return std::nullopt;
	}

	return LocaleRequest{
		*id,
		lp->priority,
		std::filesystem::path(*id).extension() == ".btloc"
	};
}

/**
 * @brief Hands resolver bytes to LocalizationManager in the right format.
 */
inline auto commitLocale(
	LocalizationManager& locMan,
	const std::string& id,
	std::vector<U8>& bytes,
	I32 priority,
	bool compiled
) {
	return compiled
		? locMan.loadFromPackBytes(bytes, priority, id)
		: locMan.loadFromMemory(bytes, priority, id);
}

} // namespace Detail

/**
 * @brief Synchronous locale loader. Reads through the resolver.
 */
class BLACKTHORN_API LocalizationSourceLoader final : public Assets::IAssetLoader<LocalizationSource> {
public:
	explicit LocalizationSourceLoader(LocalizationManager& lm)
		: locMan(lm)
	{}

	std::unique_ptr<LocalizationSource> load(const Assets::LoadParams& params) override {
		if (!resolver) {
			BT_ERROR("LocalizationSourceLoader: no resolver, loader was not registered");
			return nullptr;
		}

		const auto request = Detail::localeRequest(params);
		if (!request)
			return nullptr;

		auto bytes = resolver->resolve(request->id);
		if (!bytes)
			return nullptr;

		auto result = Detail::commitLocale(locMan, request->id, bytes->bytes, request->priority, request->compiled);
		if (!result.handle) {
			BT_ERROR("LocalizationSourceLoader: failed to load '{}' (status {})",
				request->id, static_cast<int>(result.status));
			return nullptr;
		}

		return std::make_unique<LocalizationSource>(*result.handle, locMan);
	}

private:
	LocalizationManager& locMan;
};

/**
 * @brief Asynchronous locale loader. Reads on a worker, parses on the main thread.
 */
class BLACKTHORN_API AsyncLocalizationSourceLoader final : public Assets::IAsyncAssetLoader<LocalizationSource> {
public:
	explicit AsyncLocalizationSourceLoader(LocalizationManager& lm)
		: locMan(lm)
	{}

	std::unique_ptr<Assets::IRawAssetData> loadRaw(const Assets::LoadParams& params) override {
		if (!resolver) {
			BT_ERROR("AsyncLocalizationSourceLoader: no resolver, loader was not registered");
			return nullptr;
		}

		const auto request = Detail::localeRequest(params);
		if (!request)
			return nullptr;

		auto bytes = resolver->resolve(request->id);
		if (!bytes)
			return nullptr;

		auto raw = std::make_unique<RawLocalizationSourceData>();
		raw->bytes = std::move(bytes->bytes);
		raw->priority = request->priority;
		raw->compiled = request->compiled;
		raw->valid = true;
		return raw;
	}

	void upload(Assets::IRawAssetData& rawBase, Assets::AssetManager& manager) override {
		auto& raw = static_cast<RawLocalizationSourceData&>(rawBase);

		auto result = Detail::commitLocale(locMan, raw.assetID, raw.bytes, raw.priority, raw.compiled);
		if (!result.handle) {
			BT_ERROR("AsyncLocalizationSourceLoader: '{}' failed to load (status {})",
				raw.assetID, static_cast<int>(result.status));
			return;
		}

		manager.add<LocalizationSource>(raw.assetID, std::make_unique<LocalizationSource>(*result.handle, locMan));
		BT_DEBUG("AsyncLocalizationSourceLoader: '{}' ready", raw.assetID);
	}

private:
	LocalizationManager& locMan;
};

} // namespace Blackthorn::Localization
