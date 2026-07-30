#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <limits>
#include <string>
#include <sstream>
#include <vector>

enum ExitCodes {
	Ok = 0,
	BadArgs = 2,
	MetricsError = 3,
	ImageError = 4,
	WriteError = 5
};

struct CLIOptions {
	bool dryRun = false;
	bool help = false;
	bool quiet = false;
	bool verbose = false;
	bool version = false;

	float baselineOverride = -1.0f;
	float lineHeightOverride = -1.0f;
	float spaceWidthOverride = -1.0f;

	std::string outputPath;

	std::vector<std::string> positional;
};

struct FontMetaData {
	float baseline = 0.0f;
	float lineHeight = 0.0f;
	float spaceWidth = 0.0f;
};

struct FontGlyph {
	uint32_t codePoint;
	float x, y, w, h;
	int16_t xOffset, yOffset, xAdvance;
};

void printUsage() {
	std::cout << "Usage: btfpacker [options] <font_image> <metrics_file> -o <output>\n";
	std::cout << "\nPacks a font image and metrics file into a binary .btf file.\n";
	std::cout << "\nOptions:\n";
	std::cout << "  -h, --help\t\tShows this help message.\n";
	std::cout << "  -q, --quiet\t\tSuppress non-error output.\n";
	std::cout << "  -v, --verbose\t\tShow verbose output.\n";
	std::cout << "  -o, --output <file>\tOutput BTF file.\n";
	std::cout << "  --version\t\tDisplay version information.\n";
	std::cout << "  --dry-run\t\tValidate and parse, but do not write output.\n";
	std::cout << "  --baseline\t<val>\tOverride baseline.\n";
	std::cout << "  --line-height <val>\tOverride line height.\n";
	std::cout << "  --space-width <val>\tOverride space width.\n";
	std::cout << "\nMetrics Format:\n";
	std::cout << "  common lineHeight=N [baseline=N]\n";
	std::cout << "  char id=N x=N y=N width=N height=N xoffset=N yoffset=N xadvance=N\n";
}

inline void toLower(std::string& s) {
	std::transform(s.begin(), s.end(), s.begin(), ::tolower);
}

inline void trim(std::string& s) {
	const auto first = s.find_first_not_of(" \t");
	if (first == std::string::npos) {
		s.clear();
		return;
	}

	const auto last = s.find_last_not_of(" \t");
	s = s.substr(first, last - first + 1);
}

bool parseKeyValue(const std::string& line, const std::string& key, std::string& outValue) {
	std::istringstream iss(line);
	std::string token;

	while (iss >> token) {
		auto eq = token.find('=');

		if (eq != std::string::npos) {
			std::string k = token.substr(0, eq);
			toLower(k);

			std::string v = token.substr(eq + 1);

			if (k == key) {
				if (!v.empty()) {
					v.erase(v.find_last_not_of(" \t") + 1);
					outValue = v;
					return true;
				}

				return false;
			}
		}

		if (token == key) {
			std::string eqToken;
			if (!(iss >> eqToken) || eqToken != "=")
				continue;

			if (iss >> outValue)
				return true;
		}
	}

	return false;
}

template <typename T>
bool parseNumericValue(const std::string& line, const std::string& key, int lineNum, T& outValue) {
	static_assert(std::is_arithmetic_v<T>);

	std::string value;
	if (!parseKeyValue(line, key, value))
		return false;

	try {
		if constexpr (std::is_integral_v<T>) {
			long long tmp = std::stoll(value);

			if (tmp < std::numeric_limits<T>::min() || tmp > std::numeric_limits<T>::max()) {
				std::cerr << "Error: Integer value out of range for key '" << key << "' on line " << lineNum << '\n';
				return false;
			}

			outValue = static_cast<T>(tmp);
		} else {
			long double tmp = std::stold(value);

			if (!std::isfinite(tmp)) {
				std::cerr << "Error: Non-finite float for key '" << key << "' on line " << lineNum << '\n';
				return false;
			}

			outValue = static_cast<T>(tmp);
		}

		return true;
	} catch (const std::invalid_argument&) {
		std::cerr << "Error: Invalid numeric value for key '" << key << "' on line " << lineNum << '\n';
		return false;
	} catch (const std::out_of_range&) {
		std::cerr << "Error: Numeric value out of range for key '" << key << "' on line " << lineNum << '\n';
		return false;
	}
}

bool parseFloat(const std::string& str, float& outFloat) {
	try {
		outFloat = std::stof(str);
		return true;
	} catch(std::invalid_argument&) {
		return false;
	}
}

bool parseMetrics(const std::string& path, FontMetaData& metadata, std::vector<FontGlyph>& glyphs, const CLIOptions& opts) {
	std::ifstream file(path);

	if (!file) {
		std::cerr << "Error: Failed to open metrics file: " << path << '\n';
		return false;
	}

	std::string line;
	int lineNum = 0;

	while(std::getline(file, line)) {
		lineNum++;

		size_t commentPos = line.find('#');
		if (commentPos != std::string::npos)
			line = line.substr(0, commentPos);

		trim(line);

		if (line.empty())
			continue;

		std::istringstream iss(line);
		std::string command;
		iss >> command;
		toLower(command);

		if (command == "common" || command == "global") {
			if (opts.lineHeightOverride == -1.0f) {
				if (!parseNumericValue(line, "lineheight", lineNum, metadata.lineHeight)) {
					if (opts.verbose && !opts.quiet) {
						std::cerr << "Error: Common value 'lineHeight' not found in metrics\n";
						std::cout << "  No lineHeight found in metrics file; use --line-height <value> to provide one.\n";
					}
				} else {
					if (opts.verbose && !opts.quiet)
						std::cout << "Found common: lineHeight=" << metadata.lineHeight << '\n';
				}
			}

			if (opts.baselineOverride == -1.0f) {
				parseNumericValue(line, "baseline", lineNum, metadata.baseline);

				if (metadata.baseline == 0.0f)
					parseNumericValue(line, "base", lineNum, metadata.baseline);

				if (opts.verbose && !opts.quiet && metadata.baseline != 0.0f)
					std::cout << "Found common: baseline=" << metadata.baseline << '\n';
			}
		} else if (command == "char") {
			FontGlyph glyph = {};
			bool ok = true;

			ok &= (parseNumericValue(line, "id", lineNum, glyph.codePoint));

			if (glyph.codePoint == 0) {
				if (opts.verbose && !opts.quiet)
					std::cerr << "Warning: Skipping glyph with id=0 on line " << lineNum << '\n';

				continue;
			}

			ok &= (parseNumericValue(line, "x", lineNum, glyph.x));
			ok &= (parseNumericValue(line, "y", lineNum, glyph.y));

			ok &= (parseNumericValue(line, "width", lineNum, glyph.w))
				|| (parseNumericValue(line, "w", lineNum, glyph.w));

			ok &= (parseNumericValue(line, "height", lineNum, glyph.h))
				|| (parseNumericValue(line, "h", lineNum, glyph.h));

			ok &= (parseNumericValue(line, "xoffset", lineNum, glyph.xOffset));
			ok &= (parseNumericValue(line, "yoffset", lineNum, glyph.yOffset));
			ok &= (parseNumericValue(line, "xadvance", lineNum, glyph.xAdvance));

			if (!ok) {
				if (opts.verbose && !opts.quiet)
					std::cerr << "Warning: Skipping glyph on line " << lineNum << " due to missing required fields\n";

				continue;
			}

			if (glyph.w < 0 || glyph.h < 0) {
				if (opts.verbose && !opts.quiet)
					std::cerr << "Warning: Skipping glyph with id=" << glyph.codePoint << " on line " << lineNum << " with negative dimensions\n";
				continue;
			}

			if (glyph.xAdvance < 0) {
				if (opts.verbose && !opts.quiet)
					std::cerr << "Warning: Skipping glyph with id=" << glyph.codePoint << " on line " << lineNum << " with non-positive advance\n";
				continue;
			}

			glyphs.push_back(glyph);
		} else if (command == "info" || command == "page" || command == "kerning") {
			continue;
		} else {
			if (!opts.quiet && opts.verbose)
				std::cerr << "Warning: Skipping unknown command '" << command << "' on line " << lineNum << '\n';
		}
	}

	auto spaceIt = std::find_if(glyphs.begin(), glyphs.end(),
		[](const FontGlyph& g) { return g.codePoint == 32; }
	);

	if (opts.spaceWidthOverride == -1.0f) {
		if (spaceIt != glyphs.end()) {
			metadata.spaceWidth = spaceIt->xAdvance;
		} else if (metadata.lineHeight > 0.0f) {
			metadata.spaceWidth = metadata.lineHeight * 0.25f;
		}
	}

	if (opts.baselineOverride == -1.0f && metadata.baseline == 0.0f && metadata.lineHeight > 0.0f) {
		float maxBearing = 0.0f;

		for (const auto& g : glyphs)
			maxBearing = std::max(maxBearing, static_cast<float>(-g.yOffset));

		if (maxBearing > 0.0f) {
			metadata.baseline = maxBearing;
		} else {
			metadata.baseline = metadata.lineHeight * 0.25f;
		}

		if (opts.verbose && !opts.quiet)
			std::cout << "Baseline not specified, calculated as " << metadata.baseline << ".\n";
	}

	if (opts.lineHeightOverride != -1.0f) {
		metadata.lineHeight = opts.lineHeightOverride;

		if (opts.verbose && !opts.quiet)
			std::cout << "Using lineHeight override " << metadata.lineHeight << '\n';
	}

	if (opts.baselineOverride != -1.0f) {
		metadata.baseline = opts.baselineOverride;

		if (opts.verbose && !opts.quiet)
			std::cout << "Using baseline override " << metadata.baseline << '\n';
	}

	if (opts.spaceWidthOverride != -1.0f) {
		metadata.spaceWidth = opts.spaceWidthOverride;

		if (opts.verbose && !opts.quiet)
			std::cout << "Using spaceWidth override " << metadata.spaceWidth << '\n';
	}

	if (opts.verbose && !opts.quiet)
		std::cout << "Parsed " << glyphs.size() << " glyphs from metrics.\n";

	return !glyphs.empty();
}

bool parseArguments(int argc, char const *argv[], CLIOptions& opts) {
	size_t i = 1;

	while (i < static_cast<size_t>(argc)) {
		std::string arg = argv[i];

		if (arg == "-h" || arg == "--help") {
			opts.help = true;
		} else if (arg == "-q" || arg == "--quiet") {
			opts.quiet = true;
		} else if (arg == "-v" || arg == "--verbose") {
			opts.verbose = true;
		} else if (arg == "--dry-run") {
			opts.dryRun = true;
		} else if (arg == "--version") {
			opts.version = true;
		} else if (arg == "--baseline") {
			if (++i >= static_cast<size_t>(argc)) {
				std::cerr << "Error: Missing value for --baseline\n";
				return false;
			}

			if (!parseFloat(argv[i], opts.baselineOverride)) {
				std::cerr << "Error: Invalid number for --baseline\n";
				return false;
			}
		} else if (arg == "--line-height") {
			if (++i >= static_cast<size_t>(argc)) {
				std::cerr << "Error: Missing value for --line-height\n";
				return false;
			}

			if (!parseFloat(argv[i], opts.lineHeightOverride)) {
				std::cerr << "Error: Invalid number for --line-height\n";
				return false;
			}
		} else if (arg == "--space-width") {
			if (++i >= static_cast<size_t>(argc)) {
				std::cerr << "Error: Missing value for --space-width\n";
				return false;
			}

			if (!parseFloat(argv[i], opts.spaceWidthOverride)) {
				std::cerr << "Error: Invalid number for --space-width\n";
				return false;
			}
		} else if (arg == "-o" || arg == "--output") {
			if (++i >= static_cast<size_t>(argc)) {
				std::cerr << "Error: missing value for " << arg << '\n';
				return false;
			}

			if (!opts.outputPath.empty() && !opts.quiet)
				std::cerr << "Duplicate output path specified, using " << argv[i] << '\n';

			opts.outputPath = argv[i];
		} else if (!arg.empty() && arg[0] == '-') {
			std::cerr << "Error: Unrecognized option " << arg << "\n";
			return false;
		} else {
			opts.positional.push_back(arg);
		}

		i++;
	}

	if (opts.positional.size() < 2 && !opts.help && !opts.version) {
		printUsage();
		return false;
	}

	return true;
}

bool readImageFile(const std::string& path, std::vector<uint8_t>& imageData, const CLIOptions& opts) {
	std::ifstream file(path, std::ios::binary);

	if (!file) {
		std::cerr << "Error: Failed to open image file: " << path << '\n';
		return false;
	}

	imageData.assign(std::istreambuf_iterator<char>(file), {});

	if (imageData.empty()) {
		std::cerr << "Error: Image file is empty\n";
		return false;
	}

	if (opts.verbose && !opts.quiet)
		std::cout << "Read " << imageData.size() << " bytes of image data.\n";

	return true;
}

bool writeBTF(const FontMetaData& metadata, const std::vector<uint8_t>& imageData, const std::vector<FontGlyph>& glyphs, const CLIOptions& opts) {
	if (opts.dryRun) {
		if (opts.verbose && !opts.quiet)
			std::cout << "Dry run enabled — skipping output file write\n";

		return true;
	}

	if (metadata.lineHeight <= 0.0f) {
		std::cerr << "Error: Metrics file does not define a valid lineHeight\n";
		std::cout << "  Use --line-height <value> to provide one.\n";
		return false;
	}

	if (glyphs.empty()) {
		std::cerr << "Error: No glyphs to write\n";
		return false;
	}

	if (imageData.empty()) {
		std::cerr << "Error: No image data\n";
		return false;
	}

	std::ofstream file(opts.outputPath, std::ios::binary);

	if (!file) {
		std::cerr << "Error: Failed to create output file: " << opts.outputPath << '\n';
		return false;
	}

	const char sign[4] = {'B', 'T', 'F', '\0'};
	file.write(sign, 4);

	uint16_t version = 1;
	file.write(reinterpret_cast<const char*>(&version), sizeof(version));

	file.write(reinterpret_cast<const char*>(&metadata.lineHeight), sizeof(float));
	file.write(reinterpret_cast<const char*>(&metadata.baseline), sizeof(float));
	file.write(reinterpret_cast<const char*>(&metadata.spaceWidth), sizeof(float));

	uint32_t imageSize = static_cast<uint32_t>(imageData.size());
	file.write(reinterpret_cast<const char*>(&imageSize), sizeof(imageSize));
	file.write(reinterpret_cast<const char*>(imageData.data()), imageSize);

	uint32_t glyphCount = static_cast<uint32_t>(glyphs.size());
	file.write(reinterpret_cast<const char*>(&glyphCount), sizeof(glyphCount));

	for (const auto& g : glyphs) {
		file.write(reinterpret_cast<const char*>(&g.codePoint), sizeof(g.codePoint));
		file.write(reinterpret_cast<const char*>(&g.x), sizeof(g.x));
		file.write(reinterpret_cast<const char*>(&g.y), sizeof(g.y));
		file.write(reinterpret_cast<const char*>(&g.w), sizeof(g.w));
		file.write(reinterpret_cast<const char*>(&g.h), sizeof(g.h));
		file.write(reinterpret_cast<const char*>(&g.xOffset), sizeof(g.xOffset));
		file.write(reinterpret_cast<const char*>(&g.yOffset), sizeof(g.yOffset));
		file.write(reinterpret_cast<const char*>(&g.xAdvance), sizeof(g.xAdvance));
	}

	if (!file.good()) {
		std::cerr << "Error: Failed while writing output file\n";
		return false;
	}

	if (!opts.quiet) {
		if (opts.verbose && !opts.quiet) {
			std::cout << "Successfully wrote BTF file: " << opts.outputPath << '\n';
			std::cout << "  Version: " << version << '\n';
			std::cout << "  Line Height: " << metadata.lineHeight << '\n';
			std::cout << "  Baseline: " << metadata.baseline << '\n';
			std::cout << "  Space Width: " << metadata.spaceWidth << '\n';
			std::cout << "  Image Size: " << imageSize << " bytes\n";
			std::cout << "  Glyph Count: " << glyphCount << '\n';
		} else {
			std::cout << "Wrote " << opts.outputPath << " (" << glyphCount << " glyphs, " << imageSize << " bytes image).\n";
		}
	}

	return true;
}

int main(int argc, char const *argv[]) {
	CLIOptions options;

	if (!parseArguments(argc, argv, options)) {
		return ExitCodes::BadArgs;
	}

	if (options.help) {
		printUsage();
		return ExitCodes::Ok;
	}

	if (options.version) {
		std::cout << "btfpacker version 2.0.0\n";
		return ExitCodes::Ok;
	}

	if (options.positional.size() > 2) {
		std::cerr << "Error: Too many positional arguments, expected 2, got " << options.positional.size() << "\n";
		return ExitCodes::BadArgs;
	}

	if (options.outputPath.empty()) {
		std::cerr << "Error: Output path missing\n";
		return ExitCodes::WriteError;
	}

	if (options.quiet && options.verbose) {
		std::cerr << "Warning: Both --quiet and --verbose specified, quiet takes precedence\n";
	}

	std::string imagePath = options.positional[0];
	std::string metricsPath = options.positional[1];

	FontMetaData metadata = {};
	std::vector<FontGlyph> glyphs;

	if (!parseMetrics(metricsPath, metadata, glyphs, options)) {
		std::cerr << "Error: Failed to parse metrics file\n";
		return ExitCodes::MetricsError;
	}

	std::vector<uint8_t> imageData;
	if (!readImageFile(imagePath, imageData, options)) {
		std::cerr << "Error: Failed to read image file\n";
		return ExitCodes::ImageError;
	}

	if (!writeBTF(metadata, imageData, glyphs, options)) {
		std::cerr << "Error: Failed to write BTF file\n";
		return ExitCodes::WriteError;
	}

	return ExitCodes::Ok;
}