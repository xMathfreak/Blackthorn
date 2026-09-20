#include "CLI.h"
#include "Terminal.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>

#include "Manifest.h"
#include "ManifestGenerator.h"
#include "ManifestParser.h"
#include "Packer.h"

namespace cli = Blackthorn::Tools;
namespace term = Blackthorn::Terminal;

namespace {

/// @brief Supplementary help content with no equivalent in the option/
/// positional schema, printed after the auto-generated top-level help.
constexpr const char* kSupplementaryHelp =
	"Asset ID derivation (generate-manifest):\n"
	"  IDs are derived from each file's path relative to the scanned root.\n"
	"  Path separators, spaces, and hyphens become underscores; the extension\n"
	"  is stripped; all characters are lowercased; other non-alphanumeric\n"
	"  characters are dropped.\n"
	"\n"
	"  Examples:\n"
	"    assets/shaders/default.vert  ->  shaders_default_vert\n"
	"    assets/fonts/Bebas Neue.ttf  ->  fonts_bebas_neue\n"
	"    assets/sound.ogg             ->  sound\n"
	"\n"
	"Command examples:\n"
	"  btpacker generate-manifest --assets assets/ --output data/base.btp --manifest data/base.pack.json\n"
	"  btpacker generate-manifest --assets assets/ --output data/base.btp --manifest data/base.pack.json --exclude video --exclude tmp\n"
	"  btpacker pack         --manifest data/base.pack.json\n"
	"  btpacker pack         --manifest data/base.pack.json --level 6\n"
	"  btpacker verify        data/base.btp\n"
	"  btpacker list          data/base.btp\n"
	"  btpacker unpack        data/base.btp ./unpacked\n";


void printCommandHelp(const cli::Command& command, const cli::ParseResult& result) {
	if (auto topic = result.helpTopic())
		std::cout << command.help(*topic);
	else
		std::cout << command.help();
}

int cmdPack(const cli::ParseResult& args) {
	const std::string manifestPath = args.get<std::string>("manifest");

	auto manifest = BTPacker::ManifestParser::parse(manifestPath);
	if (!manifest)
		return 1;

	if (args.specified("level"))
		manifest->compressionLevel = args.get<int>("level");

	if (args.get<bool>("no-symbols"))
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

int cmdGenManifest(const cli::ParseResult& args) {
	BTPacker::ManifestGenerator::Options opts;
	opts.assetDir = args.get<std::string>("assets");
	opts.btpOutput = args.get<std::string>("output");
	opts.manifestOut = args.get<std::string>("out");
	opts.compressionLevel = args.get<int>("level");
	opts.writeSymbolTable = !args.get<bool>("no-symbols");

	if (args.specified("exclude"))
		opts.excludeDirs = args.get_all<std::string>("exclude");

	std::cout << "scanning '" << opts.assetDir.string() << "'...\n";

	const bool ok = BTPacker::ManifestGenerator::generate(opts, std::cout);
	if (ok) {
		std::cout << "\ndone.\n";
	} else {
		std::cerr << "\nbtpacker: gen-manifest failed.\n";
	}

	return ok ? 0 : 1;
}

int cmdVerify(const cli::ParseResult& args) {
	const std::string& btpPath = args.positional("file");

	if (!std::filesystem::exists(btpPath)) {
		std::cerr << term::colorize("btpacker: error: ", term::Color::Red, stderr) << "file not found: '" << btpPath << "'\n";
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

int cmdList(const cli::ParseResult& args) {
	const std::string& btpPath = args.positional("file");

	if (!std::filesystem::exists(btpPath)) {
		std::cerr << term::colorize("btpacker: error: ", term::Color::Red, stderr) << "file not found: '" << btpPath << "'\n";
		return 1;
	}

	const bool ok = BTPacker::Packer::list(btpPath, std::cout);
	return ok ? 0 : 1;
}

int cmdUnpack(const cli::ParseResult& args) {
	const std::string& btpPath = args.positional("file");
	const std::string outDir = args.get<std::string>("out");

	if (!std::filesystem::exists(btpPath)) {
		std::cerr << term::colorize("btpacker: error: ", term::Color::Red, stderr) << "file not found: '" << btpPath << "'\n";
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
	cli::Command app{"btpacker", "Blackthorn asset pack tool"};

	cli::Command& packCmd = app.command("pack", "Read a manifest and produce a .btp pack file.");
	packCmd.option("manifest", 'm')
		.value<std::string>()
		.help("Path to the .pack.json manifest.")
		.required();
	packCmd.option("level", 'l')
		.value<int>()
		.help("zstd compression level. Overrides the manifest value. Lower = faster "
			"decompression; higher = smaller file. Default: 3.")
		.range(1, 22);
	packCmd.option("no-symbols")
		.flag()
		.help("Do not write a debug symbol table.");

	cli::Command& genManifestCmd = app.command("generate-manifest", "Scan an asset directory and auto-generate a manifest.");
	genManifestCmd.option("assets", 'a')
		.value<std::string>()
		.help("Asset directory to scan recursively.")
		.required();
	genManifestCmd.option("output", 'o')
		.value<std::string>()
		.help("Path to the .btp file to record in the manifest.")
		.required();
	genManifestCmd.option("manifest", 'm')
		.value<std::string>()
		.help("Path to write the generated manifest.")
		.required();
	genManifestCmd.option("exclude", 'x')
		.value<std::string>()
		.help("Skip a subdirectory by name. May be repeated, e.g. --exclude video --exclude tmp.")
		.repeatable();
	genManifestCmd.option("level", 'l')
		.value<int>()
		.help("Compression level written into the manifest.")
		.defaultValue(3)
		.range(1, 22);
	genManifestCmd.option("no-symbols")
		.flag()
		.help("Write 'symbol_table: false' into the manifest.");

	cli::Command& verifyCmd = app.command("verify", "Check every entry in a .btp file for corruption.");
	verifyCmd.positional("file").help("Path to the .btp file.").required();

	cli::Command& listCmd = app.command("list", "Print the table of contents of a .btp file.");
	listCmd.positional("file").help("Path to the .btp file.").required();

	cli::Command& unpackCmd = app.command("unpack", "Decompress all assets from a .btp file to disk.");
	unpackCmd.positional("file").help("Path to the .btp file.").required();
	unpackCmd.positional("output")
		.help("Directory to write decompressed assets into.")
		.required();

	// Maps a subcommand name to both its Command (for usage()/help()) and its
	// handler (for dispatch).
	const std::unordered_map<std::string, cli::Command*> commands = {
		{"pack", &packCmd},
		{"gen-manifest", &genManifestCmd},
		{"verify", &verifyCmd},
		{"list", &listCmd},
		{"unpack", &unpackCmd},
	};
	const std::unordered_map<std::string, int (*)(const cli::ParseResult&)> handlers = {
		{"pack", &cmdPack},
		{"gen-manifest", &cmdGenManifest},
		{"verify", &cmdVerify},
		{"list", &cmdList},
		{"unpack", &cmdUnpack},
	};

	auto result = app.parse(argc, argv);
	if (!result) {
		std::cerr << term::colorize("btpacker: error: ", term::Color::Red, stderr) << result.error().message << '\n';
		return 1;
	}

	// Walk down to whichever command level actually has usageRequested()/
	// helpRequested() set.
	const cli::Command* activeCommand = &app;
	const cli::ParseResult* activeResult = &*result;
	while (const cli::ParseResult* child = activeResult->subcommand()) {
		auto it = commands.find(std::string(child->command_name()));
		if (it == commands.end())
			break;

		activeCommand = it->second;
		activeResult = child;
	}

	if (result->usageRequested()) {
		std::cout << activeCommand->usage();
		return 0;
	}

	if (result->helpRequested()) {
		printCommandHelp(*activeCommand, *activeResult);

		if (activeCommand == &app && !activeResult->helpTopic())
			std::cout << '\n' << kSupplementaryHelp;

		return 0;
	}

	const cli::ParseResult* sub = result->subcommand();
	return handlers.at(std::string(sub->command_name()))(*sub);
}
