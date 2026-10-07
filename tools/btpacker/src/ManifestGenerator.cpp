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

namespace BTPacker {

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
	if (!std::filesystem::exists(opts.assetDir)) {
		std::cerr << "btpacker: error: asset directory not found: '"
				  << opts.assetDir.string() << "'\n";

		return false;
	}

	if (!std::filesystem::is_directory(opts.assetDir)) {
		std::cerr << "btpacker: error: '" << opts.assetDir.string()
				  << "' is not a directory\n";

		return false;
	}

	const std::set<std::string> excludeSet(
		opts.excludeDirs.begin(),
		opts.excludeDirs.end()
	);

	std::map<std::string, ManifestAsset> collected;
	std::vector<std::filesystem::path> metadataPaths;

	std::error_code ec;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(
		opts.assetDir,
		std::filesystem::directory_options::skip_permission_denied,
		ec
	)) {
		if (ec) {
			std::cerr << "btpacker: warning: iteration error: "
					  << ec.message() << "\n";
			ec.clear();
			continue;
		}

		if (!entry.is_regular_file())
			continue;

		const auto path = entry.path();

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

		const auto relativePath = std::filesystem::relative(path, opts.assetDir, ec);
		if (ec || relativePath.empty()) {
			std::cerr << "btpacker: warning: cannot compute relative path for '"
					  << path.string() << "', skipping\n";
			ec.clear();
			continue;
		}

		if (ext == ".metadata") {
			metadataPaths.push_back(relativePath);
			continue;
		}

		const std::string key = relativePath.generic_string();
		if (collected.contains(key)) {
			std::cerr << "btpacker: warning: duplicate path '" << key
					  << "', keeping first\n";
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
		std::cerr << "btpacker: warning: no recognised assets found\n";

	std::vector<ManifestAsset> assets;
	assets.reserve(collected.size());
	for (auto& [key, asset] : collected)
		assets.push_back(std::move(asset));

	std::sort(metadataPaths.begin(), metadataPaths.end());

	std::filesystem::path metadataPath;
	if (!metadataPaths.empty())
		metadataPath = metadataPaths.front();

	if (metadataPaths.size() > 1) {
		std::cerr << "btpacker: warning: found multiple .metadata files in '"
				  << opts.assetDir.string() << "', using '"
				  << metadataPath.generic_string() << "'\n";
	}

	return writeManifest(opts, assets, log, metadataPath);
}

bool ManifestGenerator::writeManifest(
	const Options& opts,
	const std::vector<ManifestAsset>& assets,
	std::ostream& log,
	std::filesystem::path metadataPath
) {
	const auto outDir = opts.manifestOut.parent_path();
	if (!outDir.empty()) {
		std::error_code ec;
		std::filesystem::create_directories(outDir, ec);
		if (ec) {
			std::cerr << "btpacker: error: cannot create manifest directory '"
					  << outDir.string() << "': " << ec.message() << "\n";
			return false;
		}
	}

	std::ofstream out(opts.manifestOut);
	if (!out.is_open()) {
		std::cerr << "btpacker: error: cannot write manifest to '"
				  << opts.manifestOut.string() << "'\n";
		return false;
	}

	const auto manifestDir = opts.manifestOut.parent_path().empty()
		? std::filesystem::current_path()
		: std::filesystem::absolute(opts.manifestOut.parent_path());

	std::error_code ec;
	const auto relativeBtp = std::filesystem::relative(
		std::filesystem::absolute(opts.btpOutput),
		manifestDir,
		ec
	);
	const std::string btpOutput = (!ec && !relativeBtp.empty())
		? relativeBtp.generic_string()
		: opts.btpOutput.generic_string();

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

	const auto absoluteAssetDir = std::filesystem::absolute(opts.assetDir, ec);
	const std::string sourceRoot = (!ec && !absoluteAssetDir.empty())
		? absoluteAssetDir.generic_string()
		: opts.assetDir.generic_string();

	nlohmann::ordered_json json;
	json["output"] = btpOutput;
	json["source_root"] = sourceRoot;
	json["compressionLevel"] = opts.compressionLevel;
	json["symbol_table"] = opts.writeSymbolTable;
	json["metadata"] = metadataPath.generic_string();
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

	log << "\n"
		<< "  manifest: " << opts.manifestOut.string() << "\n"
		<< "  assets:   " << assets.size() << "\n"
		<< "  metadata: "
		<< (metadataPath.empty() ? "no" : '\'' + metadataPath.string() + '\'');

	return true;
}

} // namespace BTPacker
