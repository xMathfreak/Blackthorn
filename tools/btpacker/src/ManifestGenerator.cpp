#include "ManifestGenerator.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <vector>

#include <nlohmann/json.hpp>

#include "AssetNormalize.h"
#include "AssetReference.h"

namespace BTPacker {

namespace {

void error(std::string_view message) {
	std::cerr << "btpacker: error: " << message << '\n';
}

void warn(std::string_view message) {
	std::cerr << "btpacker: warning: " << message << '\n';
}

} // namespace

std::string ManifestGenerator::classifyExtension(const std::string& ext) {
	static const std::set<std::string> textures = {
		".png", ".jpg", ".jpeg", ".bmp", ".tga", ".hdr", ".webp"
	};
	static const std::set<std::string> audio = {
		".ogg", ".wav", ".mp3", ".flac", ".opus"
	};
	static const std::set<std::string> shaders = {
		".vert", ".frag", ".geom", ".comp", ".tesc", ".tese",
		".glsl", ".hlsl", ".wgsl", ".spv"
	};
	static const std::set<std::string> fonts = {
		".ttf", ".otf", ".woff", ".woff2", ".btf"
	};
	static const std::set<std::string> spriteClips = {
		".btclip"
	};
	static const std::set<std::string> effects = {
		".btfx"
	};
	static const std::set<std::string> locale = {
		".btloc"
	};

	if (ext == ".metadata")
		return "Metadata";

	if (textures.contains(ext))
		return "Texture";

	if (audio.contains(ext))
		return "Audio";

	if (shaders.contains(ext))
		return "Shader";

	if (fonts.contains(ext))
		return "Font";

	if (spriteClips.contains(ext))
		return "SpriteClip";

	if (effects.contains(ext))
		return "ParticleEffect";

	if (locale.contains(ext))
		return "Localization";

	return "Raw";
}

bool ManifestGenerator::generate(const Options& opts, std::ostream& log) {
	std::error_code ec;

	const auto assetsDirectory = std::filesystem::absolute(opts.input, ec).lexically_normal();
	if (ec || !std::filesystem::is_directory(assetsDirectory)) {
		error("asset directory '" + opts.input.string() + "' not found");
		return false;
	}

	const std::set<std::string> excludeSet(
		opts.excludeDirs.begin(),
		opts.excludeDirs.end()
	);

	std::map<std::string, ManifestAsset> collected;
	std::vector<std::string> metadataPaths;

	for (const auto& entry : std::filesystem::recursive_directory_iterator(
		assetsDirectory,
		std::filesystem::directory_options::skip_permission_denied,
		ec
	)) {
		if (ec) {
			warn("iteration error: " + ec.message());
			ec.clear();
			continue;
		}

		if (!entry.is_regular_file())
			continue;

		const auto& path = entry.path();
		bool excluded = false;

		for (const auto& part : path) {
			if (excludeSet.contains(part.string())) {
				excluded = true;
				break;
			}
		}

		if (excluded)
			continue;

		std::string ext = path.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(),
			[](unsigned char c) {
				return static_cast<char>(std::tolower(c));
			}
		);

		const std::string type = classifyExtension(ext);
		if (type == "Raw") {
			log << "  skip  '" << path.filename().string()
				<< "'  (unrecognised extension '" << ext << "')\n";
			continue;
		}

		const auto relativePath = std::filesystem::relative(path, assetsDirectory, ec);
		if (ec) {
			warn("cannot compute path for '" + path.string() + "', skipping");
			ec.clear();
			continue;
		}

		const auto normalizedKey = Blackthorn::normalizeAssetPath(relativePath.generic_string());
		if (!normalizedKey) {
			warn("'" + relativePath.generic_string() + "' is not a valid asset path, skipping");
			continue;
		}

		const std::string key = *normalizedKey;

		if (ext == ".metadata") {
			metadataPaths.push_back(key);
			continue;
		}

		if (collected.contains(key)) {
			warn("duplicate path '" + key + "', keeping first");
			continue;
		}

		ManifestAsset asset;
		asset.sourcePath = key;
		asset.typeStr = type;

		collected.emplace(key, std::move(asset));
		log << "  found "
			<< std::left << std::setw(17) << ('[' + type + ']')
			<< key << "\n";
	}

	if (collected.empty())
		warn("no recognized assets found");

	std::vector<ManifestAsset> assets;
	assets.reserve(collected.size());
	for (auto& [k, v] : collected)
		assets.push_back(std::move(v));

	std::sort(metadataPaths.begin(), metadataPaths.end());
	std::string metadataPath;
	if (!metadataPaths.empty())
		metadataPath = metadataPaths.front();

	if (metadataPaths.size() > 1)
		warn("found multiple .metadata files, using '" + metadataPath + '\'');

	return writeManifest(opts, assets, log, metadataPath);
}

bool ManifestGenerator::writeManifest(
	const Options& opts,
	const std::vector<ManifestAsset>& assets,
	std::ostream& log,
	const std::string& metadataPath
) {
	std::string rootDirectory = opts.root;
	if (rootDirectory.empty()) {
		std::filesystem::path input(opts.input);
		if (input.filename().empty())
			input = input.parent_path();

		rootDirectory = input.filename().generic_string();
	}

	if (!Blackthorn::isValidRootName(rootDirectory)) {
		error("root name '" + rootDirectory + "' is invalid: use a single name that isn't 'root', with no '/', '\\', ':' or '@'");
		return false;
	}

	std::error_code ec;

	const auto outputDirectory = opts.output.parent_path();
	if (!outputDirectory.empty()) {
		std::filesystem::create_directories(outputDirectory, ec);
		if (ec) {
			error("cannot create manifest directory '" + outputDirectory.string() + "': " + ec.message());
			return false;
		}
	}

	std::ofstream out(opts.output);
	if (!out.is_open()) {
		error("cannot write manifest to '" + opts.output.string() + "'");
		return false;
	}

	const auto manifestDirectory = std::filesystem::absolute(
		outputDirectory.empty() ? std::filesystem::path(".") : outputDirectory,
		ec
	).lexically_normal();


	const std::vector<std::string> typeOrder = {
		"Texture",
		"Shader",
		"Audio",
		"Font",
		"SpriteClip",
		"ParticleEffect",
		"Localization",
		"Raw"
	};

	std::map<std::string, std::vector<const ManifestAsset*>> byType;
	for (const auto& asset : assets)
		byType[asset.typeStr].push_back(&asset);

	nlohmann::ordered_json json;
	json["root"] = rootDirectory;
	json["compression_level"] = opts.compressionLevel;
	json["symbol_table"] = opts.writeSymbolTable;

	if (!metadataPath.empty())
		json["metadata"] = metadataPath;

	json["assets"] = nlohmann::json::array();

	for (const auto& type : typeOrder) {
		const auto it = byType.find(type);
		if (it == byType.end() || it->second.empty())
			continue;

		for (const ManifestAsset* asset : it->second) {
			json["assets"].push_back({
				{"path", asset->sourcePath.generic_string()},
				{"type", asset->typeStr}
			});
		}
	}

	out << json.dump(2);

	out.close();
	return true;
}

} // namespace BTPacker
