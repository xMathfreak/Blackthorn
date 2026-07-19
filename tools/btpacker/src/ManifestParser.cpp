#include "ManifestParser.h"

#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

namespace BTPacker {

std::optional<PackManifest> ManifestParser::parse(const std::filesystem::path& path) {
	std::ifstream file(path, std::ios::in);
	if (!file.is_open()) {
		std::cerr << "btpacker: error: cannot open manifest '" << path.string() << "'\n";
		return std::nullopt;
	}

	std::ostringstream ss;
	ss << file.rdbuf();

	const std::filesystem::path manifestDir = path.parent_path();

	ManifestParser parser(ss.str(), manifestDir.empty() ? std::filesystem::current_path() : manifestDir);
	parser.manifest.manifestDir = parser.manifestDir;

	if (!parser.run())
		return std::nullopt;

	return std::move(parser.manifest);
}

void ManifestParser::skipWS() {
	while (position < source.size()) {
		const char c = source[position];

		if (c == '\n')
			++line;

		if (std::isspace(static_cast<unsigned char>(c))) {
			++position;
		} else if (c == '/' && position + 1 < source.size() && source[position + 1] == '/') {
			while (position < source.size() && source[position] != '\n')
				++position;
		} else {
			break;
		}
	}
}

char ManifestParser::peek() const {
	return (position < source.size()) ? source[position] : '\0';
}

char ManifestParser::consume() {
	if (position >= source.size())
		return '\0';

	const char c = source[position++];

	if (c == '\n')
		++line;

	return c;
}

bool ManifestParser::expect(char ch) {
	skipWS();
	if (peek() != ch) {
		std::ostringstream msg;
		msg << "expected '" << ch << "', got ";
		const char got = peek();
		if (got == '\0')
			msg << "end of file";
		else
			msg << "'" << got << "'";
		error(msg.str());
		return false;
	}
	consume();
	return true;
}

bool ManifestParser::parseString(std::string& out) {
	skipWS();
	if (peek() != '"') {
		error("expected '\"' to begin string");
		return false;
	}
	consume();

	out.clear();
	while (position < source.size()) {
		const char c = consume();
		if (c == '"')
			return true;

		if (c == '\\') {
			const char esc = consume();
			switch (esc) {
				case '"':
					out += '"';
					break;
				case '\\':
					out += '\\';
					break;
				case '/':
					out += '/';
					break;
				case 'n':
					out += '\n';
					break;
				case 'r':
					out += '\r';
					break;
				case 't':
					out += '\t';
					break;
				default:
					out += '\\';
					out += esc;
					break;
			}
		} else {
			out += c;
		}
	}

	error("unterminated string");
	return false;
}

bool ManifestParser::parseInt(int& out) {
	skipWS();

	bool negative = false;
	if (peek() == '-') {
		negative = true;
		consume();
	}

	if (!std::isdigit(static_cast<unsigned char>(peek()))) {
		error("expected integer");
		return false;
	}

	out = 0;
	while (std::isdigit(static_cast<unsigned char>(peek())))
		out = out * 10 + (consume() - '0');

	if (negative)
		out = -out;

	return true;
}

bool ManifestParser::parseBool(bool& out) {
	skipWS();

	if (position + 4 <= source.size() && source.substr(position, 4) == "true") {
		position += 4;
		out = true;
		return true;
	}

	if (position + 5 <= source.size() && source.substr(position, 5) == "false") {
		position += 5;
		out = false;
		return true;
	}

	error("expected 'true' or 'false'");
	return false;
}

void ManifestParser::error(const std::string& msg) const {
	std::cerr << "btpacker: manifest:" << line << ": error: " << msg << "\n";
}

bool ManifestParser::run() {
	return parseTopLevel();
}

bool ManifestParser::parseTopLevel() {
	if (!expect('{'))
		return false;

	skipWS();

	while (peek() != '}' && peek() != '\0') {
		std::string key;
		if (!parseString(key))
			return false;

		if (!expect(':'))
			return false;

		skipWS();

		if (key == "output") {
			std::string val;
			if (!parseString(val))
				return false;

			manifest.outputPath = std::filesystem::path(val);

			if (manifest.outputPath.is_relative())
				manifest.outputPath = manifestDir / manifest.outputPath;

		} else if (key == "compression_level") {
			int val = 3;
			if (!parseInt(val))
				return false;
			if (val < 1 || val > 22) {
				error("compression_level must be between 1 and 22");
				return false;
			}
			manifest.compressionLevel = val;

		} else if (key == "symbol_table") {
			bool val = true;
			if (!parseBool(val))
				return false;
			manifest.writeSymbolTable = val;

		} else if (key == "assets") {
			if (!parseAssetsArray())
				return false;

		} else {
			std::string dummy;
			if (peek() == '"') {
				if (!parseString(dummy))
					return false;

			} else if (peek() == '{') {
				int depth = 0;
				while (position < source.size()) {
					const char c = consume();

					if (c == '{') {
						++depth;
					} else if (c == '}') {
						--depth;

						if (depth == 0)
							break;
					}
				}
			} else {
				while (position < source.size() && !std::isspace(static_cast<unsigned char>(peek()))
					&& peek() != ',' && peek() != '}')
					consume();
			}
		}

		skipWS();
		if (peek() == ',') {
			consume();
			skipWS();
		}
	}

	if (!expect('}'))
		return false;

	if (manifest.outputPath.empty()) {
		error("missing required field 'output'");
		return false;
	}

	if (manifest.assets.empty()) {
		error("'assets' array is missing or empty");
		return false;
	}

	return true;
}

bool ManifestParser::parseAssetsArray() {
	if (!expect('['))
		return false;

	skipWS();

	while (peek() != ']' && peek() != '\0') {
		ManifestAsset asset;
		if (!parseAssetObject(asset))
			return false;

		manifest.assets.push_back(std::move(asset));

		skipWS();
		if (peek() == ',') {
			consume();
			skipWS();
		}
	}

	return expect(']');
}

bool ManifestParser::parseAssetObject(ManifestAsset& out) {
	if (!expect('{'))
		return false;

	skipWS();

	while (peek() != '}' && peek() != '\0') {
		std::string key;
		if (!parseString(key))
			return false;

		if (!expect(':'))
			return false;

		skipWS();

		if (key == "id") {
			if (!parseString(out.id))
				return false;

		} else if (key == "path") {
			std::string pathStr;
			if (!parseString(pathStr))
				return false;

			std::filesystem::path p(pathStr);
			out.sourcePath = p.is_relative() ? (manifestDir / p) : p;

		} else if (key == "type") {
			if (!parseString(out.typeStr))
				return false;

		} else {
			std::string dummy;
			if (peek() == '"') {
				if (!parseString(dummy))
					return false;

			} else {
				while (position < source.size() && !std::isspace(static_cast<unsigned char>(peek()))
					&& peek() != ',' && peek() != '}')
					consume();
			}
		}

		skipWS();
		if (peek() == ',') {
			consume();
			skipWS();
		}
	}

	if (!expect('}'))
		return false;

	if (out.id.empty()) {
		error("asset object missing required field 'id'");
		return false;
	}

	if (out.sourcePath.empty()) {
		error("asset '" + out.id + "' missing required field 'path'");
		return false;
	}

	if (out.typeStr.empty()) {
		error("asset '" + out.id + "' missing required field 'type'");
		return false;
	}

	return true;
}

} // namespace BTPacker