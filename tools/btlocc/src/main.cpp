#include "CLI.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>

#include "Compiler.h"

namespace cli = Blackthorn::Tools;

namespace {

/// @brief Supplementary help content with no equivalent in the option/
/// positional schema, printed after the auto-generated top-level help.
constexpr const char* kSupplementaryHelp =
	"Locale JSON schema:\n"
	"  { \"localeCode\": \"en-US\", \"entries\": {\n"
	"      \"inventory.empty\": \"Your inventory is empty\",\n"
	"      \"inventory.potions_found\": { \"one\": \"You found {count} potion.\",\n"
	"                                    \"other\": \"You found {count} potions.\" },\n"
	"      \"ui.continue\": { \"text\": \"Continue\", \"context\": \"Pause menu button\" }\n"
	"  } }\n"
	"\n"
	"Command examples:\n"
	"  btlocc compile en-US.json\n"
	"  btlocc compile en-US.json de-DE.json fr-FR.json\n"
	"  btlocc compile en-US.json de-DE.json --base en-US.json --out assets/loc/\n"
	"  btlocc compile en-US.json --header include/Game/LocIDs.h --namespace Game::Loc\n"
	"  btlocc compile en-US.json de-DE.json --strict\n";

void printCommandHelp(const cli::Command& command, const cli::ParseResult& result) {
	if (auto topic = result.helpTopic())
		std::cout << command.help(*topic);
	else
		std::cout << command.help();
}

int cmdCompile(const cli::ParseResult& args) {
	BTLocC::CompileOptions opts;

	for (const auto& p : args.positional_all("files"))
		opts.files.emplace_back(p);

	if (args.specified("base"))
		opts.basePath = std::filesystem::path(args.get<std::string>("base"));

	opts.outDir = args.get<std::string>("out");
	opts.compressionLevel = args.get<int>("level");
	opts.strict = args.get<bool>("strict");
	opts.writeSymbolTable = !args.get<bool>("no-symbols");

	if (args.specified("header"))
		opts.headerPath = std::filesystem::path(args.get<std::string>("header"));

	opts.headerNamespace = args.get<std::string>("namespace");

	std::cout << "compiling " << opts.files.size() << " locale file(s)...\n";

	const bool ok = BTLocC::Compiler::compile(opts, std::cout);
	if (ok) {
		std::cout << "\ndone.\n";
	} else {
		std::cerr << "\nbtlocc: compile failed.\n";
	}

	return ok ? 0 : 1;
}

} // anonymous namespace

int main(int argc, char** argv) {
	cli::Command app{"btlocc", "Blackthorn localization compiler"};

	cli::Command& compileCmd = app.command("compile", "Compile locale JSON file(s) into .btloc, and optionally a TextID header.");

	compileCmd.positional("files")
		.help("Locale JSON file(s) to compile. The first one given is the base/canonical "
			"locale unless --base names a different one. A single file needs no options: "
			"'btlocc compile en-US.json' just compiles it.")
		.required()
		.repeatable();

	compileCmd.option("base", 'b')
		.value<std::string>()
		.help("Which of the given files is the base/canonical locale (defaults to the "
			"first positional). Its key set is authoritative for cross-checking and, if "
			"--header is given, for the generated TextID header.");
	compileCmd.option("out", 'o')
		.value<std::string>()
		.help("Output directory. Each locale is written as <out>/<localeCode>.btloc.")
		.defaultValue(std::string("."));
	compileCmd.option("level")
		.value<int>()
		.help("zstd compression level. Lower = faster decompression; higher = smaller file. Default: 3.")
		.defaultValue(3)
		.range(1, 22);
	compileCmd.option("strict")
		.flag()
		.help("Escalate base/translation key-set mismatches from warnings to a hard failure.");
	compileCmd.option("no-symbols")
		.flag()
		.help("Do not embed a debug symbol table (TextID -> original key) in the .btloc file.");
	compileCmd.option("header")
		.value<std::string>()
		.help("Also generate a C++ header of TextID constants, one per key in the base locale.");
	compileCmd.option("namespace")
		.value<std::string>()
		.help("C++ namespace for the generated header's constants. Default: Loc.")
		.defaultValue(std::string("Loc"));

	// Maps a subcommand name to both its Command (for usage()/help()) and its
	// handler (for dispatch).
	const std::unordered_map<std::string, cli::Command*> commands = {
		{"compile", &compileCmd},
	};
	const std::unordered_map<std::string, int (*)(const cli::ParseResult&)> handlers = {
		{"compile", &cmdCompile},
	};

	auto result = app.parse(argc, argv);
	if (!result) {
		std::cerr << "btlocc: error: " << result.error().message << '\n';
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
