#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "Manifest.h"
#include "ManifestGenerator.h"
#include "ManifestParser.h"
#include "Packer.h"

namespace {

void printUsage() {
	std::cout <<
		"btpacker - Blackthorn asset pack tool\n"
		"\n"
		"Usage:\n"
		"  btpacker pack         --manifest <manifest.pack.json> [--level <1-22>] [--no-symbols]\n"
		"  btpacker gen-manifest --assets <dir> --output <file.btp> --out <manifest.pack.json>\n"
		"                        [--exclude <dir>] [--level <1-22>] [--no-symbols]\n"
		"  btpacker verify       <file.btp>\n"
		"  btpacker list         <file.btp>\n"
		"  btpacker unpack       <file.btp> --out <directory>\n"
		"\n"
		"Commands:\n"
		"  pack          Read a manifest and produce a .btp pack file.\n"
		"  gen-manifest  Scan an asset directory and auto-generate a manifest.\n"
		"  verify        Check every entry in a .btp file for corruption.\n"
		"  list          Print the table of contents of a .btp file.\n"
		"  unpack        Decompress all assets from a .btp file to disk.\n"
		"\n"
		"Pack options:\n"
		"  --manifest <path>   Path to the .pack.json manifest (required).\n"
		"  --level <1-22>      zstd compression level. Overrides the manifest value.\n"
		"                      Lower = faster decompression; higher = smaller file.\n"
		"                      Default: 3.\n"
		"  --no-symbols        Do not write a debug symbol table.\n"
		"\n"
		"gen-manifest options:\n"
		"  --assets <dir>      Asset directory to scan recursively (required).\n"
		"  --output <file.btp> Value of the 'output' field in the manifest (required).\n"
		"                      This is where btpacker pack will write the .btp file.\n"
		"  --out <path>        Path to write the generated manifest (required).\n"
		"  --exclude <name>    Skip a subdirectory by name. May be repeated.\n"
		"                      e.g. --exclude video --exclude tmp\n"
		"  --level <1-22>      Compression level written into the manifest. Default: 3.\n"
		"  --no-symbols        Write 'symbol_table: false' into the manifest.\n"
		"\n"
		"Unpack options:\n"
		"  --out <directory>   Directory to write decompressed assets into (required).\n"
		"\n"
		"Asset ID derivation (gen-manifest):\n"
		"  IDs are derived from each file's path relative to the scanned root.\n"
		"  Path separators, spaces, and hyphens become underscores; the extension\n"
		"  is stripped; all characters are lowercased; non-alphanumeric characters\n"
		"  are dropped.\n"
		"\n"
		"  Examples:\n"
		"    assets/shaders/default.vert  ->  shaders_default_vert\n"
		"    assets/fonts/Bebas Neue.ttf  ->  fonts_bebas_neue\n"
		"    assets/sound.ogg             ->  sound\n"
		"\n"
		"Examples:\n"
		"  btpacker gen-manifest --assets assets/ --output data/base.btp --out data/base.pack.json\n"
		"  btpacker gen-manifest --assets assets/ --output data/base.btp --out data/base.pack.json --exclude video --exclude tmp\n"
		"  btpacker pack   --manifest data/base.pack.json\n"
		"  btpacker pack   --manifest data/base.pack.json --level 6\n"
		"  btpacker verify data/base.btp\n"
		"  btpacker list   data/base.btp\n"
		"  btpacker unpack data/base.btp --out ./unpacked\n";
}

/**
 * @brief Looks for a named flag in the argument list.
 * @return true if @p flag is present.
 */
bool hasFlag(const std::vector<std::string>& args, const std::string& flag) {
	for (const auto& a : args)
		if (a == flag)
			return true;

	return false;
}

/**
 * @brief Returns the value of a named option, or an empty string if absent.
 *
 * Handles both "--key value" (two separate tokens) and "--key=value" (single
 * token with an '=' separator).
 */
std::string getOption(const std::vector<std::string>& args, const std::string& key) {
	for (size_t i = 0; i < args.size(); ++i) {
		if (args[i] == key && i + 1 < args.size())
			return args[i + 1];

		const std::string prefix = key + "=";
		if (args[i].size() > prefix.size() && args[i].substr(0, prefix.size()) == prefix)
			return args[i].substr(prefix.size());
	}

	return {};
}

/**
 * @brief Collects all values for a repeated option (e.g. multiple --exclude flags).
 *
 * @param args  Full argument list.
 * @param key   Option name (e.g. "--exclude").
 * @return All values associated with @p key, in order.
 */
std::vector<std::string> getOptionAll(
	const std::vector<std::string>& args,
	const std::string& key
) {
	std::vector<std::string> results;
	const std::string prefix = key + "=";

	for (size_t i = 0; i < args.size(); ++i) {
		if (args[i] == key && i + 1 < args.size()) {
			results.push_back(args[i + 1]);
		} else if (args[i].size() > prefix.size() && args[i].substr(0, prefix.size()) == prefix) {
			results.push_back(args[i].substr(prefix.size()));
		}
	}

	return results;
}

int cmdPack(const std::vector<std::string>& args) {
	const std::string manifestPath = getOption(args, "--manifest");
	if (manifestPath.empty()) {
		std::cerr << "btpacker: error: 'pack' requires --manifest <path>\n\n";
		printUsage();
		return 1;
	}

	auto manifest = BTPacker::ManifestParser::parse(manifestPath);
	if (!manifest)
		return 1;

	const std::string levelStr = getOption(args, "--level");
	if (!levelStr.empty()) {
		try {
			const int level = std::stoi(levelStr);
			if (level < 1 || level > 22) {
				std::cerr << "btpacker: error: --level must be between 1 and 22\n";
				return 1;
			}
			manifest->compressionLevel = level;
		} catch (...) {
			std::cerr << "btpacker: error: invalid --level value '" << levelStr << "'\n";
			return 1;
		}
	}

	if (hasFlag(args, "--no-symbols"))
		manifest->writeSymbolTable = false;

	std::cout << "packing '" << manifest->outputPath.string() << "'...\n";

	const bool ok = BTPacker::Packer::pack(*manifest, std::cout);
	if (ok) {
		std::cout << "\ndone.\n";
	} else {
		std::cerr << "\nbtpacker: pack failed.\n";
	}

	return ok ? 0 : 1;
}

int cmdGenManifest(const std::vector<std::string>& args) {
	const std::string assetsStr = getOption(args, "--assets");
	const std::string outputStr = getOption(args, "--output");
	const std::string outStr = getOption(args, "--out");

	if (assetsStr.empty() || outputStr.empty() || outStr.empty()) {
		std::cerr << "btpacker: error: 'gen-manifest' requires --assets, --output, and --out\n\n";
		printUsage();
		return 1;
	}

	BTPacker::ManifestGenerator::Options opts;
	opts.assetDir = assetsStr;
	opts.btpOutput = outputStr;
	opts.manifestOut = outStr;
	opts.excludeDirs = getOptionAll(args, "--exclude");
	opts.writeSymbolTable = !hasFlag(args, "--no-symbols");

	const std::string levelStr = getOption(args, "--level");
	if (!levelStr.empty()) {
		try {
			const int level = std::stoi(levelStr);
			if (level < 1 || level > 22) {
				std::cerr << "btpacker: error: --level must be between 1 and 22\n";
				return 1;
			}

			opts.compressionLevel = level;
		} catch (...) {
			std::cerr << "btpacker: error: invalid --level value '" << levelStr << "'\n";
			return 1;
		}
	}

	std::cout << "scanning '" << opts.assetDir.string() << "'...\n";

	const bool ok = BTPacker::ManifestGenerator::generate(opts, std::cout);
	if (ok) {
		std::cout << "\ndone.\n";
	} else {
		std::cerr << "\nbtpacker: gen-manifest failed.\n";
	}

	return ok ? 0 : 1;
}

int cmdVerify(const std::vector<std::string>& args) {
	std::string btpPath;
	for (const auto& a : args) {
		if (a.empty() || a[0] == '-')
			continue;

		btpPath = a;
		break;
	}

	if (btpPath.empty()) {
		std::cerr << "btpacker: error: 'verify' requires a .btp file path\n\n";
		printUsage();
		return 1;
	}

	if (!std::filesystem::exists(btpPath)) {
		std::cerr << "btpacker: error: file not found: '" << btpPath << "'\n";
		return 1;
	}

	const bool ok = BTPacker::Packer::verify(btpPath, std::cout);
	if (ok) {
		std::cout << "\nall entries OK.\n";
	} else {
		std::cerr << "\nbtpacker: verify found errors.\n";
	}

	return ok ? 0 : 1;
}

int cmdList(const std::vector<std::string>& args) {
	std::string btpPath;
	for (const auto& a : args) {
		if (a.empty() || a[0] == '-')
			continue;

		btpPath = a;
		break;
	}

	if (btpPath.empty()) {
		std::cerr << "btpacker: error: 'list' requires a .btp file path\n\n";
		printUsage();
		return 1;
	}

	if (!std::filesystem::exists(btpPath)) {
		std::cerr << "btpacker: error: file not found: '" << btpPath << "'\n";
		return 1;
	}

	const bool ok = BTPacker::Packer::list(btpPath, std::cout);
	return ok ? 0 : 1;
}

int cmdUnpack(const std::vector<std::string>& args) {
	std::string btpPath;
	for (const auto& a : args) {
		if (a.empty() || a[0] == '-')
			continue;

		btpPath = a;
		break;
	}

	const std::string outDir = getOption(args, "--out");

	if (btpPath.empty()) {
		std::cerr << "btpacker: error: 'unpack' requires a .btp file path\n\n";
		printUsage();
		return 1;
	}

	if (outDir.empty()) {
		std::cerr << "btpacker: error: 'unpack' requires --out <directory>\n\n";
		printUsage();
		return 1;
	}

	if (!std::filesystem::exists(btpPath)) {
		std::cerr << "btpacker: error: file not found: '" << btpPath << "'\n";
		return 1;
	}

	const bool ok = BTPacker::Packer::unpack(btpPath, outDir, std::cout);

	if (ok) {
		std::cout << "\ndone.\n";
	} else {
		std::cerr << "\nbtpacker: unpack encountered errors.\n";
	}

	return ok ? 0 : 1;
}

} // anonymous namespace

int main(int argc, char** argv) {
	if (argc < 2) {
		printUsage();
		return 1;
	}

	const std::string command = argv[1];

	std::vector<std::string> args;
	args.reserve(static_cast<size_t>(argc - 2));
	for (int i = 2; i < argc; ++i)
		args.emplace_back(argv[i]);

	if (command == "pack")
		return cmdPack(args);

	if (command == "gen-manifest")
		return cmdGenManifest(args);

	if (command == "verify")
		return cmdVerify(args);

	if (command == "list")
		return cmdList(args);

	if (command == "unpack")
		return cmdUnpack(args);

	if (command == "--help" || command == "-h" || command == "help") {
		printUsage();
		return 0;
	}

	std::cerr << "btpacker: error: unknown command '" << command << "'\n\n";
	printUsage();
	return 1;
}