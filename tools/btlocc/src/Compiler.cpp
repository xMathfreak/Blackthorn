#include "Compiler.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

#define XXH_INLINE_ALL
#include <xxhash.h>

#include <zstd.h>

#include "LocManifest.h"
#include "LocParser.h"
#include "Localization/LocFormat.h"

using namespace Blackthorn::Localization;

namespace BTLocC {

namespace {

	uint64_t hashKey(std::string_view key) {
		return XXH64(key.data(), key.size(), 0);
	}

	/// Sanitizes a JSON key into a valid C++ identifier: non-alphanumeric,
	/// non-underscore characters become '_'; a leading digit gets a '_'
	/// prefix. Casing is preserved, so the generated constant still visually
	/// corresponds to its JSON key (e.g. "inventory.potions_found" ->
	/// "inventory_potions_found").
	std::string sanitizeIdentifier(std::string_view key) {
		std::string out;
		out.reserve(key.size());

		for (const char c : key)
			out += (std::isalnum(static_cast<unsigned char>(c)) || c == '_') ? c : '_';

		if (out.empty() || std::isdigit(static_cast<unsigned char>(out[0])))
			out.insert(out.begin(), '_');

		return out;
	}

	/// Duplicate JSON keys within one file, and TextID hash collisions
	/// between distinct keys (astronomically unlikely at 64 bits, but cheap
	/// to check for and much worse to ship silently than to catch here).
	bool checkKeysAndCollisions(const LocFile& file) {
		std::unordered_set<std::string> seenKeys;
		std::unordered_map<uint64_t, std::string> seenIDs;
		bool ok = true;

		for (const auto& [key, value] : file.entries) {
			(void)value;

			if (!seenKeys.insert(key).second) {
				std::cerr << file.sourcePath << ": error: duplicate key '" << key << "'\n";
				ok = false;
				continue;
			}

			const uint64_t id = hashKey(key);
			if (const auto it = seenIDs.find(id); it != seenIDs.end()) {
				std::cerr << file.sourcePath << ": error: TextID collision between '"
					<< it->second << "' and '" << key << "', rename one of these keys\n";
				ok = false;
				continue;
			}

			seenIDs.emplace(id, key);
		}

		return ok;
	}

	/// Warns (or, with strict=true, errors) about keys present in one of
	/// base/translation but not the other.
	bool crossCheckKeys(const LocFile& base, const std::unordered_set<std::string>& baseKeys, const LocFile& translation, bool strict) {
		std::unordered_set<std::string> translationKeys;
		for (const auto& [key, value] : translation.entries) {
			(void)value;
			translationKeys.insert(key);
		}

		bool mismatch = false;

		for (const auto& key : baseKeys) {
			if (!translationKeys.contains(key)) {
				std::cerr << translation.sourcePath << ": warning: missing key '" << key
					<< "' (present in base locale '" << base.localeCode << "')\n";
				mismatch = true;
			}
		}

		for (const auto& key : translationKeys) {
			if (!baseKeys.contains(key)) {
				std::cerr << translation.sourcePath << ": warning: extra key '" << key
					<< "' not present in base locale '" << base.localeCode << "'\n";
				mismatch = true;
			}
		}

		if (mismatch && strict) {
			std::cerr << translation.sourcePath << ": error: key mismatches against the base locale are fatal with --strict\n";
			return false;
		}

		return true;
	}

	/// Appends one symbol table record (see LocFormat.h / LocalizationManager
	/// ::readSymbolTable): uint64 textID, uint16 keyLen, then keyLen raw bytes
	/// (not null-terminated). Mirrors btpacker's own symbol table encoding,
	/// minus the source-path field .btloc has no use for.
	void appendSymbolEntry(std::vector<char>& buf, uint64_t id, const std::string& key) {
		const uint16_t keyLen = static_cast<uint16_t>(std::min<size_t>(key.size(), 0xFFFFu));
		if (keyLen != key.size()) {
			std::cerr << "btlocc: warning: key '" << key
				<< "' is longer than 65535 bytes and will be truncated in the debug symbol table\n";
		}

		const size_t idStart = buf.size();
		buf.resize(idStart + sizeof(uint64_t));
		std::memcpy(buf.data() + idStart, &id, sizeof(uint64_t));

		const size_t lenStart = buf.size();
		buf.resize(lenStart + sizeof(uint16_t));
		std::memcpy(buf.data() + lenStart, &keyLen, sizeof(uint16_t));

		const size_t keyStart = buf.size();
		buf.resize(keyStart + keyLen);
		std::memcpy(buf.data() + keyStart, key.data(), keyLen);
	}

	bool writeBTLOC(const LocFile& file, const std::filesystem::path& outPath, int level, bool writeSymbols, std::ostream& log) {
		std::vector<BTLOCEntry> tocEntries;
		tocEntries.reserve(file.entries.size());
		std::vector<char> stringBlob;
		std::vector<char> symbolTable;

		// Returns a (offset, length) pair for @p s within stringBlob, appending
		// it (even when empty, an empty string still gets a real offset, distinct
		// from BTLOC_ABSENT_OFFSET, so a present-but-empty form or
		// context round-trips correctly instead of looking "not present").
		auto appendString = [&](const std::string& s) -> std::pair<uint64_t, uint32_t> {
			const uint64_t offset = stringBlob.size();
			stringBlob.insert(stringBlob.end(), s.begin(), s.end());
			return {offset, static_cast<uint32_t>(s.size())};
		};

		for (const auto& [key, value] : file.entries) {
			BTLOCEntry entry{};
			entry.textID = hashKey(key);

			// Every slot starts absent; only categories actually present in
			// this entry get overwritten below. (BTLOCEntry{} zero-inits,
			// and 0 is a valid real offset and is explicitly made different from
			// BTLOC_ABSENT_OFFSET).
			for (auto& off : entry.formOffset)
				off = BTLOC_ABSENT_OFFSET;
			entry.contextOffset = BTLOC_ABSENT_OFFSET;

			if (value.plainText) {
				const auto [off, len] = appendString(*value.plainText);
				const size_t idx = static_cast<size_t>(PluralCategory::Other);
				entry.formOffset[idx] = off;
				entry.formLength[idx] = len;
			} else {
				for (const auto& [category, text] : value.forms) {
					const auto [off, len] = appendString(text);
					const size_t idx = static_cast<size_t>(category);
					entry.formOffset[idx] = off;
					entry.formLength[idx] = len;
				}
			}

			// context is a plain (non-optional) std::string in LocEntryValue,
			// so an omitted "context" key and an explicit "context": "" are
			// indistinguishable at this point. Both come through as "".
			// Treating that as "no context" (rather than giving it a real
			// slot) is the simpler, and only currently meaningful, choice:
			// nothing reads Entry::context yet, so there's no observable
			// difference between the two cases to preserve.
			if (!value.context.empty()) {
				const auto [off, len] = appendString(value.context);
				entry.contextOffset = off;
				entry.contextLength = len;
			}

			tocEntries.push_back(entry);

			if (writeSymbols)
				appendSymbolEntry(symbolTable, entry.textID, key);
		}

		// Combined block layout: entries array immediately followed by the
		// string blob. Matches what LocalizationManager::parsePackFile
		// expects (see LocFormat.h).
		const size_t entriesBytes = tocEntries.size() * sizeof(BTLOCEntry);
		std::vector<char> raw(entriesBytes + stringBlob.size());
		std::memcpy(raw.data(), tocEntries.data(), entriesBytes);
		std::memcpy(raw.data() + entriesBytes, stringBlob.data(), stringBlob.size());

		std::vector<char> compressed(ZSTD_compressBound(raw.size()));
		const size_t compResult = ZSTD_compress(compressed.data(), compressed.size(), raw.data(), raw.size(), level);
		if (ZSTD_isError(compResult)) {
			std::cerr << "btlocc: error: zstd compression failed for '" << file.localeCode << "': " << ZSTD_getErrorName(compResult) << "\n";
			return false;
		}
		compressed.resize(compResult);

		BTLOCHeader header{};
		header.magic = BTLOC_MAGIC;
		header.version = BTLOC_VERSION;
		header.flags = 0;
		header.entryCount = static_cast<uint32_t>(tocEntries.size());

		std::memset(header.localeCode, 0, sizeof(header.localeCode));
		if (file.localeCode.size() >= sizeof(header.localeCode)) {
			std::cerr << "btlocc: warning: locale code '" << file.localeCode << "' is longer than "
				<< (sizeof(header.localeCode) - 1) << " characters and will be truncated in the .btloc file\n";
		}
		const size_t copyLen = std::min(file.localeCode.size(), sizeof(header.localeCode) - 1);
		std::memcpy(header.localeCode, file.localeCode.data(), copyLen);

		header.compSize = compressed.size();
		header.uncompSize = raw.size();

		if (!symbolTable.empty()) {
			header.symbolTableOff = sizeof(BTLOCHeader) + compressed.size();
			header.symbolTableSize = symbolTable.size();
		} else {
			header.symbolTableOff = 0;
			header.symbolTableSize = 0;
		}

		std::error_code ec;
		std::filesystem::create_directories(outPath.parent_path(), ec);

		std::FILE* f = std::fopen(outPath.string().c_str(), "wb");
		if (!f) {
			std::cerr << "btlocc: error: cannot create '" << outPath.string() << "'\n";
			return false;
		}

		bool ok = std::fwrite(&header, 1, sizeof(header), f) == sizeof(header)
			&& std::fwrite(compressed.data(), 1, compressed.size(), f) == compressed.size();

		if (ok && !symbolTable.empty())
			ok = std::fwrite(symbolTable.data(), 1, symbolTable.size(), f) == symbolTable.size();

		std::fclose(f);

		if (!ok) {
			std::cerr << "btlocc: error: write failed for '" << outPath.string() << "'\n";
			return false;
		}

		log << "  compiled  " << file.localeCode << "  ->  " << outPath.string()
			<< "  (" << tocEntries.size() << " entries, " << compressed.size() << " B compressed"
			<< (symbolTable.empty() ? "" : ", symbols included") << ")\n";

		return true;
	}

	bool writeHeader(const LocFile& base, const std::filesystem::path& headerPath, const std::string& ns, std::ostream& log) {
		std::error_code ec;
		std::filesystem::create_directories(headerPath.parent_path(), ec);

		std::ofstream out(headerPath);
		if (!out) {
			std::cerr << "btlocc: error: cannot create '" << headerPath.string() << "'\n";
			return false;
		}

		out << "// Generated by btlocc from '" << base.sourcePath << "'.\n";
		out << "// Do not edit this file directly, regenerate the source instead.\n\n";
		out << "#pragma once\n\n";
		out << "#include \"Localization/TextID.h\"\n\n";
		out << "namespace " << ns << " {\n\n";

		for (const auto& [key, value] : base.entries) {
			(void)value;
			const std::string ident = sanitizeIdentifier(key);
			const uint64_t id = hashKey(key);

			out << "inline constexpr Blackthorn::Localization::TextID " << ident << "{0x"
				<< std::hex << std::setfill('0') << std::setw(16) << id << std::dec
				<< "ULL}; // \"" << key << "\"\n";
		}

		out << "\n} // namespace " << ns << "\n";

		log << "  wrote header  " << headerPath.string() << "  (" << base.entries.size() << " constants)\n";
		return true;
	}

} // namespace

bool Compiler::compile(const CompileOptions& opts, std::ostream& log) {
	if (opts.files.empty()) {
		std::cerr << "btlocc: error: no locale files given\n";
		return false;
	}

	std::filesystem::path basePath = opts.files.front();
	if (opts.basePath) {
		const auto it = std::find(opts.files.begin(), opts.files.end(), *opts.basePath);
		if (it == opts.files.end()) {
			std::cerr << "btlocc: error: --base '" << opts.basePath->string()
				<< "' does not match any of the given locale files\n";
			return false;
		}
		basePath = *it;
	}

	auto base = LocParser::parse(basePath);
	if (!base)
		return false;
	base->sourcePath = basePath.string();

	if (!checkKeysAndCollisions(*base))
		return false;

	std::vector<LocFile> translations;
	translations.reserve(opts.files.size() - 1);

	for (const auto& p : opts.files) {
		if (p == basePath)
			continue;

		auto parsed = LocParser::parse(p);
		if (!parsed)
			return false;

		parsed->sourcePath = p.string();

		if (!checkKeysAndCollisions(*parsed))
			return false;

		translations.push_back(std::move(*parsed));
	}

	std::unordered_set<std::string> baseKeys;
	for (const auto& [key, value] : base->entries) {
		(void)value;
		baseKeys.insert(key);
	}

	bool hadErrors = false;
	for (const auto& t : translations) {
		if (!crossCheckKeys(*base, baseKeys, t, opts.strict))
			hadErrors = true;
	}
	if (hadErrors)
		return false;

	if (!writeBTLOC(*base, opts.outDir / (base->localeCode + ".btloc"), opts.compressionLevel, opts.writeSymbolTable, log))
		return false;

	for (const auto& t : translations) {
		if (!writeBTLOC(t, opts.outDir / (t.localeCode + ".btloc"), opts.compressionLevel, opts.writeSymbolTable, log))
			return false;
	}

	if (opts.headerPath) {
		if (!writeHeader(*base, *opts.headerPath, opts.headerNamespace, log))
			return false;
	}

	return true;
}

} // namespace BTLocC
