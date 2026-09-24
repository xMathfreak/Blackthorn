#include "LocParser.h"

#include <fstream>
#include <iostream>
#include <sstream>

using Blackthorn::Localization::PluralCategory;

namespace BTLocC {

namespace {

	std::optional<PluralCategory> parsePluralCategoryName(std::string_view name) {
		if (name == "zero") return PluralCategory::Zero;
		if (name == "one") return PluralCategory::One;
		if (name == "two") return PluralCategory::Two;
		if (name == "few") return PluralCategory::Few;
		if (name == "many") return PluralCategory::Many;
		if (name == "other") return PluralCategory::Other;
		return std::nullopt;
	}

	/// Encodes a Unicode code point (BMP only) as UTF-8, appended to out.
	void appendUTF8(std::string& out, unsigned codepoint) {
		if (codepoint <= 0x7Fu) {
			out += static_cast<char>(codepoint);
		} else if (codepoint <= 0x7FFu) {
			out += static_cast<char>(0xC0u | (codepoint >> 6));
			out += static_cast<char>(0x80u | (codepoint & 0x3Fu));
		} else {
			out += static_cast<char>(0xE0u | (codepoint >> 12));
			out += static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu));
			out += static_cast<char>(0x80u | (codepoint & 0x3Fu));
		}
	}

} // namespace

std::optional<LocFile> LocParser::parse(const std::filesystem::path& path) {
	std::ifstream f(path);
	if (!f) {
		std::cerr << path.string() << ": error: cannot open file\n";
		return std::nullopt;
	}

	std::ostringstream ss;
	ss << f.rdbuf();

	LocParser parser(ss.str(), path.string());
	if (!parser.run())
		return std::nullopt;

	if (parser.result.localeCode.empty()) {
		std::cerr << path.string() << ": error: missing required field 'localeCode'\n";
		return std::nullopt;
	}

	return parser.result;
}

bool LocParser::run() {
	skipWS();
	return parseTopLevel();
}

void LocParser::skipWS() {
	while (true) {
		const char c = peek();

		if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
			consume();
			continue;
		}

		if (c == '/' && position + 1 < source.size() && source[position + 1] == '/') {
			while (peek() != '\n' && peek() != '\0')
				consume();
			continue;
		}

		break;
	}
}

char LocParser::peek() const {
	if (position >= source.size())
		return '\0';
	return source[position];
}

char LocParser::consume() {
	const char c = peek();
	if (c == '\0')
		return c;

	++position;
	if (c == '\n')
		++line;

	return c;
}

bool LocParser::expect(char ch) {
	skipWS();
	if (peek() != ch) {
		error(std::string("expected '") + ch + "'");
		return false;
	}
	consume();
	return true;
}

bool LocParser::parseString(std::string& out) {
	skipWS();

	if (peek() != '"') {
		error("expected a string");
		return false;
	}
	consume(); // opening quote

	out.clear();

	while (true) {
		const char c = peek();

		if (c == '\0') {
			error("unterminated string");
			return false;
		}

		if (c == '"') {
			consume();
			return true;
		}

		if (c == '\\') {
			consume();
			const char esc = consume();

			switch (esc) {
				case '"': out += '"'; break;
				case '\\': out += '\\'; break;
				case '/': out += '/'; break;
				case 'n': out += '\n'; break;
				case 't': out += '\t'; break;
				case 'r': out += '\r'; break;
				case 'b': out += '\b'; break;
				case 'f': out += '\f'; break;
				case 'u': {
					unsigned code = 0;
					for (int i = 0; i < 4; ++i) {
						const char h = consume();
						code <<= 4;

						if (h >= '0' && h <= '9') code |= static_cast<unsigned>(h - '0');
						else if (h >= 'a' && h <= 'f') code |= static_cast<unsigned>(h - 'a' + 10);
						else if (h >= 'A' && h <= 'F') code |= static_cast<unsigned>(h - 'A' + 10);
						else { error("invalid \\u escape"); return false; }
					}

					appendUTF8(out, code);
					break;
				}
				default:
					error(std::string("invalid escape sequence '\\") + esc + "'");
					return false;
			}

			continue;
		}

		out += c;
		consume();
	}
}

bool LocParser::parseTopLevel() {
	if (!expect('{'))
		return false;

	skipWS();
	if (peek() == '}') {
		consume();
		return true; // Empty object; missing 'entries' is caught by the caller.
	}

	bool sawEntries = false;

	while (true) {
		std::string key;
		if (!parseString(key))
			return false;

		if (!expect(':'))
			return false;

		skipWS();

		if (key == "localeCode") {
			if (!parseString(result.localeCode))
				return false;
		} else if (key == "entries") {
			sawEntries = true;
			if (!parseEntriesObject())
				return false;
		} else {
			error("unrecognized top-level key '" + key + "' (expected 'localeCode' or 'entries')");
			return false;
		}

		skipWS();
		const char c = peek();

		if (c == ',') {
			consume();
			skipWS();
			continue;
		}

		if (c == '}') {
			consume();
			break;
		}

		error("expected ',' or '}'");
		return false;
	}

	if (!sawEntries) {
		error("missing required object 'entries'");
		return false;
	}

	return true;
}

bool LocParser::parseEntriesObject() {
	if (!expect('{'))
		return false;

	skipWS();
	if (peek() == '}') {
		consume();
		return true;
	}

	while (true) {
		std::string key;
		if (!parseString(key))
			return false;

		if (!expect(':'))
			return false;

		LocEntryValue entry;
		if (!parseEntryValue(entry))
			return false;

		result.entries.emplace_back(std::move(key), std::move(entry));

		skipWS();
		const char c = peek();

		if (c == ',') {
			consume();
			skipWS();
			continue;
		}

		if (c == '}') {
			consume();
			break;
		}

		error("expected ',' or '}'");
		return false;
	}

	return true;
}

bool LocParser::parseEntryValue(LocEntryValue& out) {
	skipWS();
	const char c = peek();

	if (c == '"') {
		std::string s;
		if (!parseString(s))
			return false;

		out.plainText = std::move(s);
		return true;
	}

	if (c != '{') {
		error("expected a string or object for entry value");
		return false;
	}

	consume(); // '{'
	skipWS();

	if (peek() == '}') {
		error("entry object defines nothing (expected 'text', plural-form keys, and/or 'context')");
		return false;
	}

	while (true) {
		std::string key;
		if (!parseString(key))
			return false;

		if (!expect(':'))
			return false;

		std::string value;
		if (!parseString(value))
			return false;

		if (key == "context") {
			out.context = std::move(value);
		} else if (key == "text") {
			out.plainText = std::move(value);
		} else {
			const auto category = parsePluralCategoryName(key);
			if (!category) {
				error("unrecognized key '" + key + "' (expected a plural category, 'text', or 'context')");
				return false;
			}
			out.forms[*category] = std::move(value);
		}

		skipWS();
		const char sep = peek();

		if (sep == ',') {
			consume();
			skipWS();
			continue;
		}

		if (sep == '}') {
			consume();
			break;
		}

		error("expected ',' or '}'");
		return false;
	}

	if (out.plainText && !out.forms.empty()) {
		error("entry has both 'text' and plural-form keys; use one or the other");
		return false;
	}

	if (!out.plainText && out.forms.empty()) {
		error("entry object defines no 'text' and no plural forms");
		return false;
	}

	return true;
}

void LocParser::error(const std::string& msg) const {
	std::cerr << path << ":" << line << ": error: " << msg << "\n";
}

} // namespace BTLocC
