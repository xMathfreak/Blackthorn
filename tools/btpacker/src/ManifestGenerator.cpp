#include "ManifestGenerator.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <set>

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
		".ttf", ".otf", ".woff", ".woff2", ".bmf"
	};
	static const std::set<std::string> spriteClips = {
		".btclip"
	};

	if (textures.count(ext))
		return "Texture";

	if (audio.count(ext))
		return "Audio";

	if (shaders.count(ext))
		return "Shader";

	if (fonts.count(ext))
		return "Font";

	if (spriteClips.count(ext))
		return "SpriteClip";

	return "Raw";
}

std::string ManifestGenerator::deriveID(const std::filesystem::path& relPath) {
	const std::filesystem::path withExt =
		relPath.parent_path() / (relPath.stem().string() + relPath.extension().string());

	std::string raw = withExt.generic_string();

	std::string id;
	id.reserve(raw.size());

	for (char c : raw) {
		if (c == '/' || c == '\\' || c == ' ' || c == '-' || c == '.') {
			if (!id.empty() && id.back() != '_')
				id += '_';
		} else if (std::isalnum(static_cast<unsigned char>(c))) {
			id += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
	}

	while (!id.empty() && id.back() == '_')
		id.pop_back();

	return id;
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

	std::set<std::string> excludeSet(opts.excludeDirs.begin(), opts.excludeDirs.end());
	std::map<std::string, ManifestAsset> collected;

	std::map<std::string, std::string> idToRelPath;

	std::error_code ec;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(
		opts.assetDir,
		std::filesystem::directory_options::skip_permission_denied,
		ec
	)) {
		if (ec) {
			std::cerr << "btpacker: warning: iteration error: " << ec.message() << "\n";
			ec.clear();
			continue;
		}

		if (!entry.is_regular_file())
			continue;

		const std::filesystem::path absPath = entry.path();

		bool excluded = false;
		for (const auto& part : absPath) {
			if (excludeSet.count(part.string())) {
				excluded = true;
				break;
			}
		}

		if (excluded)
			continue;

		std::string ext = absPath.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(),
			[](unsigned char c){ return static_cast<char>(std::tolower(c)); }
		);

		const std::string typeStr = classifyExtension(ext);

		if (typeStr == "Raw") {
			log << "  skip    " << absPath.filename().string()
				<< "  (unrecognised extension '" << ext << "')\n";
			continue;
		}

		const auto relPath = std::filesystem::relative(absPath, opts.assetDir, ec);
		if (ec || relPath.empty()) {
			std::cerr << "btpacker: warning: cannot compute relative path for '"
					  << absPath.string() << "', skipping\n";
			ec.clear();
			continue;
		}

		const std::string id = deriveID(relPath);
		if (id.empty()) {
			std::cerr << "btpacker: warning: could not derive ID for '"
					  << relPath.string() << "', skipping\n";
			continue;
		}

		const std::string key = relPath.generic_string();
		auto idIt = idToRelPath.find(id);
		if (idIt != idToRelPath.end()) {
			std::cerr << "btpacker: warning: ID collision \n'"
					  << key << "' and '" << idIt->second
					  << "' both map to id '" << id
					  << "'. Rename one or edit the manifest manually.\n";
		} else {
			idToRelPath.emplace(id, key);
		}

		if (collected.count(key)) {
			std::cerr << "btpacker: warning: duplicate path '" << key
					  << "', keeping first\n";
			continue;
		}

		ManifestAsset asset;
		asset.id = id;
		asset.sourcePath = absPath;
		asset.typeStr = typeStr;

		collected.emplace(key, std::move(asset));
		log << "  found   " << key
			<< "  [" << typeStr << "]  ->  id: " << id << "\n";
	}

	if (collected.empty())
		std::cerr << "btpacker: warning: no recognised assets found\n";

	std::vector<ManifestAsset> assets;
	assets.reserve(collected.size());
	for (auto& [key, asset] : collected)
		assets.push_back(std::move(asset));

	return writeManifest(opts, assets, log);
}

bool ManifestGenerator::writeManifest(
	const Options& opts,
	const std::vector<ManifestAsset>& assets,
	std::ostream& log
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

	const std::filesystem::path manifestDir =
		opts.manifestOut.parent_path().empty()
		? std::filesystem::current_path()
		: std::filesystem::absolute(opts.manifestOut.parent_path());

	std::error_code ec;
	const auto relBtp = std::filesystem::relative(
		std::filesystem::absolute(opts.btpOutput), manifestDir, ec
	);
	const std::string btpOutputStr = (!ec && !relBtp.empty())
		? relBtp.generic_string()
		: opts.btpOutput.generic_string();

	const std::vector<std::string> typeOrder = {
		"Texture", "Shader", "Audio", "Font", "SpriteClip", "Raw"
	};

	std::map<std::string, std::vector<const ManifestAsset*>> byType;
	for (const auto& a : assets)
		byType[a.typeStr].push_back(&a);

	std::vector<std::vector<const ManifestAsset*>*> activeGroups;
	for (const std::string& type : typeOrder) {
		auto it = byType.find(type);
		if (it != byType.end() && !it->second.empty())
			activeGroups.push_back(&it->second);
	}

	out << "{\n";
	out << "\t\"output\": \"" << btpOutputStr << "\",\n";
	out << "\t\"compression_level\": " << opts.compressionLevel << ",\n";
	out << "\t\"symbol_table\": " << (opts.writeSymbolTable ? "true" : "false") << ",\n";
	out << "\n";
	out << "\t\"assets\": [\n";

	for (size_t gi = 0; gi < activeGroups.size(); ++gi) {
		const auto& group = *activeGroups[gi];
		const bool isLast = (gi == activeGroups.size() - 1);

		out << "\t\t// ---- " << group[0]->typeStr << " ----\n";

		for (size_t ai = 0; ai < group.size(); ++ai) {
			const ManifestAsset* asset = group[ai];

			const auto relSrc = std::filesystem::relative(
				asset->sourcePath, manifestDir, ec
			);
			const std::string pathStr = (!ec && !relSrc.empty())
				? relSrc.generic_string()
				: asset->sourcePath.generic_string();

			const bool needsComma = !(isLast && ai == group.size() - 1);

			out << "\t\t{ \"id\": \"" << asset->id << "\","
				<< " \"path\": \"" << pathStr << "\","
				<< " \"type\": \"" << asset->typeStr << "\" }"
				<< (needsComma ? "," : "")
				<< "\n";
		}

		if (!isLast)
			out << "\n";
	}

	out << "\t]\n";
	out << "}\n";

	out.close();

	log << "\n"
		<< "  manifest: " << opts.manifestOut.string() << "\n"
		<< "  assets:   " << assets.size() << "\n";

	return true;
}

} // namespace BTPacker
