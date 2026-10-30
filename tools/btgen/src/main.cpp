#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

#include "CLI.h"
#include "NormalizeSymbol.h"
#include "Terminal.h"

namespace fs = std::filesystem;
namespace CLI = Blackthorn::Tools;
namespace Term = Blackthorn::Terminal;

namespace {

void error(std::string_view message) {
	std::cerr
	<< Term::colorize("btgen: error: ", Term::Color::Red, stderr)
	<< message
	<< "\n";
}

bool generate(CLI::ParseResult& result) {
	fs::path inputPath = result.positional("input");
	fs::path outputPath = result.positional("output");

	std::string symbolName = result.specified("symbol")
		? result.get<std::string>("symbol")
		: inputPath.filename().string();

	auto normalized = BTGen::normalizeVariableName(symbolName);

	std::ifstream input(inputPath, std::ios::binary);
	if (!input) {
		error("Failed to open '" + inputPath.string() + "'");
		return false;
	}

	std::vector<uint8_t> data(
		(std::istreambuf_iterator<char>(input)),
		std::istreambuf_iterator<char>()
	);

	std::string headerPath = outputPath.replace_extension(".h").generic_string();
	std::string sourcePath = outputPath.replace_extension(".cpp").generic_string();

	// Generate Header
	{
		std::ofstream header(headerPath);
		if (!header) {
			error("Failed to create header");
			return false;
		}

		header << "#pragma once\n\n";
		header << "#include <cstddef>\n\n";

		header << "namespace Blackthorn::Resources {\n\n";

		header << "extern const unsigned char ";
		header << normalized << "[];\n";

		header << "extern const size_t ";
		header << normalized << "_size;\n";

		header << "\n} // namespace Blackthorn::Resources\n";
	}

	// Generate source
	{
		std::ofstream source(sourcePath);
		if (!source) {
			error("Failed to create source");
			return false;
		}
		source << "#include \"" << headerPath << "\"\n\n";
		source << "namespace Blackthorn::Resources {\n\n";

		source << "extern const unsigned char ";
		source << normalized << "[] = {\n";

		for (size_t i = 0; i < data.size(); ++i) {
			char buffer[8];
			std::snprintf(
				buffer,
				sizeof(buffer),
				"0x%02X",
				data[i]
			);

			source << buffer;

			if (i + 1 < data.size())
				source << ", ";

			if ((i + 1) % 12 == 0 && i + 1 < data.size())
				source << "\n\t";
		}

		if (data.empty())
			source << "0x00";

		source << "\n};\n\n";

		source << "extern const size_t ";
		source << normalized << "_size = " << data.size() << ";\n";
		source << "\n} // namespace Blackthorn::Resources\n";
	}

	return true;
}

} // anonymous namespace

int main(int argc, char *argv[]) {
	CLI::Command app{"btgen", "Generates a C++ source file containing the binary contents of a resource."};

	app.positional("input")
		.help("Path to the resource")
		.required();

	app.option("symbol", 's')
		.value<std::string>()
		.help("Symbol name for the resource. Defaults to the input file name.");

	app.positional("output")
		.help("Output source path")
		.required();

	auto result = app.parse(argc, argv);
	if (!result) {
		error(result.error().message);
		return 1;
	}

	if (result->usageRequested()) {
		std::cout << app.usage();
		return 0;
	}

	if (result->helpRequested()) {
		std::cout << app.help();
		return 0;
	}

	if (!generate(*result))
		return 1;

	return 0;
}