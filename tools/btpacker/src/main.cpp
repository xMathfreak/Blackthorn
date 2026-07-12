#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "Manifest.h"
#include "ManifestParser.h"
#include "Packer.h"

namespace {

void printUsage() {
	std::cout <<
		"btpacker - Blackthorn asset pack tool\n"
		"\n"
		"Usage:\n"
		"  btpacker pack   --manifest <manifest.pack.json> [--level <1-22>] [--no-symbols]\n"
		"  btpacker verify <file.btp>\n"
		"  btpacker list   <file.btp>\n"
		"  btpacker unpack <file.btp> --out <directory>\n"
		"\n"
		"Commands:\n"
		"  pack     Read a manifest and produce a .btp pack file.\n"
		"  verify   Check every entry in a .btp file for corruption.\n"
		"  list     Print the table of contents of a .btp file.\n"
		"  unpack   Decompress all assets from a .btp file to disk.\n"
		"\n"
		"Pack options:\n"
		"  --manifest <path>   Path to the .pack.json manifest (required).\n"
		"  --level <1-22>      zstd compression level. Overrides the manifest value.\n"
		"                      Lower = faster decompression; higher = smaller file.\n"
		"                      Default: 3 (fast decompression, good ratio).\n"
		"  --no-symbols        Do not write a debug symbol table.\n"
		"\n"
		"Unpack options:\n"
		"  --out <directory>   Directory to write decompressed assets into (required).\n"
		"\n"
		"Examples:\n"
		"  btpacker pack   --manifest data/base.pack.json\n"
		"  btpacker pack   --manifest data/chapter1.pack.json --level 6\n"
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

		if (args[i].substr(0, prefix.size()) == prefix)
			return args[i].substr(prefix.size());
	}

	return {};
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
	if (ok)
		std::cout << "\nall entries OK.\n";
	else
		std::cerr << "\nbtpacker: verify found errors.\n";

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
	if (ok)
		std::cout << "\ndone.\n";
	else
		std::cerr << "\nbtpacker: unpack encountered errors.\n";

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