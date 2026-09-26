#pragma once

#include "Assets/IAssetLoader.h"
#include "Assets/AssetManager.h"
#include "Debug/Logger.h"
#include "Core/Export.h"
#include "Localization/LocalizationSource.h"
#include "Localization/LocalizationManager.h"

#ifdef BT_PACK_MODE
	#include "Assets/AssetResolver.h"
#endif

namespace Blackthorn::Localization {

class LocalizationManager;

/**
 * @brief Non-owning proxy registered with AssetManager for a locale
 * source owned by LocalizationManager.
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

	[[nodiscard]] SourceHandle getHandle() const noexcept { return handle; }

private:
	LocalizationManager& locMan;
	SourceHandle handle;
};

struct BLACKTHORN_API LocaleLoadParams final : Assets::LoadParams {
	std::filesystem::path path;
	I32 priority = 0;

	LocaleLoadParams(std::filesystem::path p, I32 pr = 0)
		: path(std::move(p))
		, priority(pr)
	{}

	std::unique_ptr<Assets::LoadParams> clone() const override {
		return std::make_unique<LocaleLoadParams>(*this);
	}
};

#ifdef BT_PACK_MODE
struct BLACKTHORN_API PackLocaleLoadParams final : Assets::LoadParams {
	std::string assetID;
	I32 priority = 0;

	PackLocaleLoadParams(std::string id, I32 pr = 0)
		: assetID(std::move(id))
		, priority(pr)
	{}

	std::unique_ptr<Assets::LoadParams> clone() const override {
		return std::make_unique<PackLocaleLoadParams>(*this);
	}
};
#endif

/// @brief Raw bytes decoded on a worker thread; either JSON text or a full `.btloc` blob.
struct BLACKTHORN_API RawLocalizationSourceData : Assets::IRawAssetData {
	std::vector<U8> bytes;
	I32 priority = 0;
};

class BLACKTHORN_API LocalizationSourceLoader
	: public Assets::IAssetLoader<LocalizationSource> {
public:
	LocalizationSourceLoader(LocalizationManager& lm)
		: locMan(lm)
	{}

	std::unique_ptr<LocalizationSource> load(const Assets::LoadParams& params) override {
		const auto* lp = dynamic_cast<const LocaleLoadParams*>(&params);

		if (!lp) {
			BT_ERROR("LocalizationSourceLoader: expected LocaleLoadParams");
			return nullptr;
		}

		const bool isCompiled = lp->path.extension() == ".btloc";
		auto result = isCompiled
			? locMan.loadFromPack(lp->path, lp->priority)
			: locMan.loadFromFile(lp->path, lp->priority);

		if (!result.handle) {
			BT_ERROR(
				"LocalizationSourceLoader: failed to load '{}' (status'{}')",
				lp->path.string(),
				static_cast<int>(result.status)
			);
			return nullptr;
		}

		return std::make_unique<LocalizationSource>(*result.handle, locMan);
	}

	std::vector<std::string> getSupportedExtensions() const override {
		return { ".btloc", ".json" };
	}

private:
	LocalizationManager& locMan;
};

class BLACKTHORN_API AsyncLocalizationSourceLoader final : public Assets::IAsyncAssetLoader<LocalizationSource> {
public:
#ifdef BT_PACK_MODE
	explicit AsyncLocalizationSourceLoader(LocalizationManager& lm, Assets::AssetResolver* resolver)
		: locMan(lm)
		, m_resolver(resolver)
	{}
#else
	AsyncLocalizationSourceLoader(LocalizationManager& lm)
		: locMan(lm)
	{}
#endif

	std::unique_ptr<Assets::IRawAssetData> loadRaw(const Assets::LoadParams& params) override {
#ifdef BT_PACK_MODE
		return loadRawFromPack(params);
#else
		return loadRawFromDisk(params);
#endif
	}

	void upload(Assets::IRawAssetData& rawBase, Assets::AssetManager& manager) override {
		auto& raw = static_cast<RawLocalizationSourceData&>(rawBase);

#ifdef BT_PACK_MODE
		auto result = locMan.loadFromPackBytes(raw.bytes, raw.priority, raw.assetID);
#else
		auto result = locMan.loadFromMemory(raw.bytes, raw.priority, raw.assetID);
#endif

		if (!result.handle) {
			BT_ERROR(
				"AsyncLocalizationSourceLoader: '{}' failed to load (status {})",
				raw.assetID,
				static_cast<int>(result.status)
			);

			return;
		}

		manager.add<LocalizationSource>(raw.assetID, std::make_unique<LocalizationSource>(*result.handle, locMan));
		BT_DEBUG(
			"AsyncLocalizationSourceLoader: '{}' ready",
			raw.assetID
		);
	}

	std::vector<std::string> getSupportedExtensions() const override {
		return { ".btloc", ".json" };
	}
private:
	LocalizationManager& locMan;

#ifdef BT_PACK_MODE
	Assets::AssetResolver* m_resolver = nullptr;

	std::unique_ptr<Assets::IRawAssetData> loadRawFromPack(const Assets::LoadParams& params) {
		const auto* pp = dynamic_cast<const PackLocaleLoadParams*>(&params);
		if (!pp) {
			BT_ERROR("AsyncLocalizationSourceLoader: BT_PACK_MODE requires PackLocaleLoadParams");
			return nullptr;
		}

		if (!m_resolver) {
			BT_ERROR("AsyncLocalizationSourceLoader: resolver is null, was registerPackLoader() used?");
			return nullptr;
		}

		auto packed = m_resolver->resolve(pp->assetID);
		if (!packed) {
			BT_ERROR("AsyncLocalizationSourceLoader: '{}' not found in any mounted pack", pp->assetID);
			return nullptr;
		}

		auto raw = std::make_unique<RawLocalizationSourceData>();
		raw->bytes = std::move(packed->bytes);
		raw->priority = pp->priority;
		raw->valid = true;
		return raw;
	}
#endif

	std::unique_ptr<Assets::IRawAssetData> loadRawFromDisk(const Assets::LoadParams& params) {
		const auto* lp = dynamic_cast<const LocaleLoadParams*>(&params);
		if (!lp) {
			BT_ERROR("AsyncLocalizationSourceLoader: expected LocaleLoadParams");
			return nullptr;
		}

		std::ifstream f(lp->path, std::ios::binary);
		if (!f) {
			BT_ERROR("AsyncLocalizationSourceLoader: cannot open '{}'", lp->path.string());
			return nullptr;
		}

		f.seekg(0, std::ios::end);
		const auto size = f.tellg();
		f.seekg(0, std::ios::beg);

		auto raw = std::make_unique<RawLocalizationSourceData>();
		raw->bytes.resize(static_cast<size_t>(size));
		raw->priority = lp->priority;
		raw->valid = static_cast<bool>(f.read(reinterpret_cast<char*>(raw->bytes.data()), size));

		if (!raw->valid)
			BT_ERROR("AsyncLocalizationSourceLoader: short read from '{}'", lp->path.string());

		return raw;
	}
};

} // namespace Blackthorn::Localization