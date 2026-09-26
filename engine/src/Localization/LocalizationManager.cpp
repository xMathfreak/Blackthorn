#include "Localization/LocalizationManager.h"

#include <algorithm>
#include <cstring>
#include <format>
#include <fstream>

#define XXH_INLINE_ALL
#include <xxhash.h>

#include <zstd.h>

#include "Debug/Logger.h"
#include "Localization/LocFormat.h"

namespace fs = std::filesystem;

namespace Blackthorn::Localization {

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

	// Parses one JSON value from an "entries" object into an Entry. Accepts:
	//   - a plain string                            -> Entry::text = string
	//   - { "text": "...", ["context": "..."] }      -> Entry::text = string
	//   - { "one": "...", "other": "...", ...,
	//       ["context": "..."] }                     -> Entry::text = PluralForms
	LoadStatus parseEntryValue(std::string_view path, std::string_view key, const nlohmann::json& value, Entry& outEntry) {
		if (value.is_string()) {
			outEntry.text = value.get<std::string>();
			return LoadStatus::Success;
		}

		if (!value.is_object()) {
			BT_ERROR("LocalizationManager: locale file '{}' entry '{}' has an unsupported JSON type (expected a string or object)", path, key);
			return LoadStatus::InvalidEntry;
		}

		if (value.contains("context"))
			outEntry.context = value["context"].get<std::string>();

		if (value.contains("text")) {
			outEntry.text = value["text"].get<std::string>();
			return LoadStatus::Success;
		}

		PluralForms forms;
		for (const auto& [formKey, formValue] : value.items()) {
			if (formKey == "context")
				continue;

			const auto category = parsePluralCategoryName(formKey);
			if (!category) {
				BT_ERROR("LocalizationManager: locale file '{}' entry '{}' has unrecognized key '{}' (expected a plural category, 'text', or 'context')", path, key, formKey);
				return LoadStatus::InvalidEntry;
			}

			if (!formValue.is_string()) {
				BT_ERROR("LocalizationManager: locale file '{}' entry '{}' plural form '{}' is not a string", path, key, formKey);
				return LoadStatus::InvalidEntry;
			}

			forms.try_emplace(*category, formValue.get<std::string>());
		}

		if (forms.empty()) {
			BT_ERROR("LocalizationManager: locale file '{}' entry '{}' is an object but defines no 'text' and no plural forms", path, key);
			return LoadStatus::InvalidEntry;
		}

		outEntry.text = std::move(forms);
		return LoadStatus::Success;
	}

	/// Human-readable label for a TextID in diagnostics: the original key when
	/// known (source.debugSymbols), otherwise the raw hash in hex so it's at
	/// least greppable against a locale file's compiled output.
	std::string describeTextID(const Source& source, TextID id) {
		if (const auto it = source.debugSymbols.find(id); it != source.debugSymbols.end())
			return it->second;

		return std::format("0x{:016X}", id.value());
	}

	/// Reads the optional debug symbol table (TextID -> original key) from an
	/// already-validated .btloc buffer (the whole file's bytes). Absent
	/// (header.symbolTableOff == 0) or corrupt/truncated tables degrade to
	/// an empty map rather than failing the whole load.
	std::unordered_map<TextID, std::string> readSymbolTable(std::span<const U8> bytes, const BTLOCHeader& header) {
		std::unordered_map<TextID, std::string> symbols;

		if (header.symbolTableOff == 0 || header.symbolTableSize == 0)
			return symbols;

		if (header.symbolTableOff + header.symbolTableSize > bytes.size())
			return symbols;

		const U8* cursor = bytes.data() + header.symbolTableOff;
		const U8* end = cursor + header.symbolTableSize;

		while (cursor < end) {
			if (cursor + sizeof(uint64_t) + sizeof(uint16_t) > end)
				break;

			uint64_t rawID = 0;
			std::memcpy(&rawID, cursor, sizeof(uint64_t));
			cursor += sizeof(uint64_t);

			uint16_t keyLen = 0;
			std::memcpy(&keyLen, cursor, sizeof(uint16_t));
			cursor += sizeof(uint16_t);

			if (cursor + keyLen > end)
				break;

			symbols.try_emplace(TextID{rawID}, std::string(reinterpret_cast<const char*>(cursor), keyLen));
			cursor += keyLen;
		}

		return symbols;
	}
} // namespace

TextID makeTextID(std::string_view id) noexcept {
	return TextID{XXH64(id.data(), id.size(), 0)};
}

LocalizationManager::LocalizationManager() = default;

LocalizationManager::~LocalizationManager() {

}

void LocalizationManager::setLocaleCode(std::string_view lc) {
	localeCode = lc;
}

std::string_view LocalizationManager::getLocaleCode() const noexcept {
	return localeCode;
}

LoadResult LocalizationManager::loadFromFile(fs::path path, I32 priority) {
	Source src;
	src.priority = priority;

	const auto result = parseLocaleFile(path, src);

	if (result != LoadStatus::Success)
		return {std::nullopt, result};

	src.handle = nextHandle++;
	const auto handle = src.handle;
	sources.emplace(handle, std::move(src));
	insertIntoPriorityOrder(handle, priority);

	return { handle, LoadStatus::Success };
}

void LocalizationManager::insertIntoPriorityOrder(SourceHandle handle, I32 priority) {
	const auto insertPos = std::find_if(priorityOrder.begin(), priorityOrder.end(),
		[this, priority](SourceHandle existing) {
			return sources.at(existing).priority < priority;
		});

	priorityOrder.insert(insertPos, handle);
}

LoadResult LocalizationManager::loadFromPack(fs::path path, I32 priority) {
	Source src;
	src.priority = priority;

	const auto result = parsePackFile(path, src);

	if (result != LoadStatus::Success)
		return {std::nullopt, result};

	src.handle = nextHandle++;
	const auto handle = src.handle;
	sources.emplace(handle, std::move(src));
	insertIntoPriorityOrder(handle, priority);

	return { handle, LoadStatus::Success };
}

LoadResult LocalizationManager::loadFromPackBytes(std::span<const U8> btlocBytes, I32 priority, std::string_view label) {
	Source src;
	src.priority = priority;

	const auto result = parsePackBytes(btlocBytes, label, src);
	if (result != LoadStatus::Success)
		return { std::nullopt, result };

	src.handle = nextHandle++;
	const auto handle = src.handle;
	sources.emplace(handle, std::move(src));
	insertIntoPriorityOrder(handle, priority);

	return { handle, LoadStatus::Success };
}

bool LocalizationManager::unloadSource(SourceHandle handle) {
	const auto it = sources.find(handle);
	if (it == sources.end())
		return false;

	sources.erase(it);

	const auto orderIt = std::find(priorityOrder.begin(), priorityOrder.end(), handle);
	if (orderIt != priorityOrder.end())
		priorityOrder.erase(orderIt);

	return true;
}

LoadStatus LocalizationManager::parseLocaleJSON(const nlohmann::json& data, std::string_view label, Source& outSource) {
	if (!data.contains("localeCode")) {
		BT_ERROR("LocalizationManager: locale file '{}' is missing required field 'localeCode'", label);
		return LoadStatus::MissingLocaleCode;
	}

	if (!data.contains("entries")) {
		BT_ERROR("LocalizationManager: locale file '{}' is missing required object 'entries'", label);
		return LoadStatus::MissingEntries;
	}

	const auto& entries = data["entries"];
	for (const auto& [key, value] : entries.items()) {
		const TextID id = makeTextID(key);

		Entry entry;
		const auto entryStatus = parseEntryValue(label, key, value, entry);
		if (entryStatus != LoadStatus::Success)
			return entryStatus;

		auto [_, inserted] = outSource.entries.try_emplace(id, std::move(entry));

		if (!inserted)
			BT_WARN("LocalizationManager: locale file '{}' contains a duplicate ID '{}'", label, key);

		outSource.debugSymbols.try_emplace(id, key);
	}

	outSource.localeCode = data["localeCode"].get<std::string>();

	return LoadStatus::Success;
}

LoadResult LocalizationManager::loadFromMemory(std::span<const U8> jsonBytes, I32 priority, std::string_view label) {
	Source src;
	src.priority = priority;

	nlohmann::json data;

	try {
		data = nlohmann::json::parse(
			reinterpret_cast<const char*>(jsonBytes.data()),
			reinterpret_cast<const char*>(jsonBytes.data()) + jsonBytes.size(),
			nullptr, true, true
		);
	} catch (const nlohmann::json::parse_error& e) {
		BT_ERROR(
			"LocalizationManager: '{}' failed to parse: {}",
			label, e.what()
		);

		return { std::nullopt, LoadStatus::InvalidJSON };
	}

	const auto result = parseLocaleJSON(data, label, src);

	if (result != LoadStatus::Success)
		return { std::nullopt, result };

	src.handle = nextHandle++;
	const auto handle = src.handle;
	sources.emplace(handle, std::move(src));
	insertIntoPriorityOrder(handle, priority);

	return { handle, LoadStatus::Success };
}

LoadStatus LocalizationManager::parsePackFile(fs::path& path, Source& outSource) {
	std::error_code ec;

	if (!fs::exists(path, ec)) {
		BT_ERROR("LocalizationManager: pack file '{}' not found", path.string());
		return LoadStatus::FileNotFound;
	}

	std::ifstream f(path, std::ios::binary);
	if (!f) {
		BT_ERROR("LocalizationManager: pack file '{}' could not be opened", path.string());
		return LoadStatus::FileNotFound;
	}

	f.seekg(0, std::ios::end);
	const auto size = f.tellg();
	f.seekg(0, std::ios::beg);

	std::vector<U8> bytes(static_cast<size_t>(size));
	if (!f.read(reinterpret_cast<char*>(bytes.data()), size)) {
		BT_ERROR("LocalizationManager: pack file '{}' could not be fully read", path.string());
		return LoadStatus::InvalidPackFile;
	}

	return parsePackBytes(bytes, path.string(), outSource);
}

LoadStatus LocalizationManager::parsePackBytes(std::span<const U8> bytes, std::string_view label, Source& outSource) {
	if (bytes.size() < sizeof(BTLOCHeader)) {
		BT_ERROR("LocalizationManager: pack '{}' is too short to contain a header", label);
		return LoadStatus::InvalidPackFile;
	}

	BTLOCHeader header{};
	std::memcpy(&header, bytes.data(), sizeof(header));

	if (header.magic != BTLOC_MAGIC) {
		BT_ERROR("LocalizationManager: pack '{}' is not a .btloc file (bad magic)", label);
		return LoadStatus::InvalidPackFile;
	}

	if (header.version != BTLOC_VERSION) {
		BT_ERROR("LocalizationManager: pack '{}' uses version {} (expected {})", label, header.version, BTLOC_VERSION);
		return LoadStatus::InvalidPackFile;
	}

	const size_t compressedOffset = sizeof(BTLOCHeader);
	if (bytes.size() < compressedOffset + header.compSize) {
		BT_ERROR("LocalizationManager: pack '{}' is truncated (compressed block)", label);
		return LoadStatus::InvalidPackFile;
	}

	const U8* compressed = bytes.data() + compressedOffset;

	std::vector<char> raw(static_cast<size_t>(header.uncompSize));
	const size_t decompressedSize = ZSTD_decompress(raw.data(), raw.size(), compressed, static_cast<size_t>(header.compSize));
	if (ZSTD_isError(decompressedSize) || decompressedSize != raw.size()) {
		BT_ERROR("LocalizationManager: pack '{}' failed to decompress: {}", label, ZSTD_getErrorName(decompressedSize));
		return LoadStatus::InvalidPackFile;
	}

	const size_t entriesBytes = static_cast<size_t>(header.entryCount) * sizeof(BTLOCEntry);
	if (entriesBytes > raw.size()) {
		BT_ERROR("LocalizationManager: pack '{}' entry table exceeds the decompressed block size", label);
		return LoadStatus::InvalidPackFile;
	}

	std::vector<BTLOCEntry> entries(header.entryCount);
	std::memcpy(entries.data(), raw.data(), entriesBytes);

	const char* stringBlob = raw.data() + entriesBytes;
	const size_t stringBlobSize = raw.size() - entriesBytes;

	outSource.debugSymbols = readSymbolTable(bytes, header);

	auto sliceOf = [&](std::string_view entryLabel, std::string_view fieldTag, uint64_t offset, uint32_t length) -> std::optional<std::string> {
		if (offset > stringBlobSize || length > stringBlobSize - offset) {
			BT_ERROR("LocalizationManager: pack '{}' entry '{}' {} has an out-of-range string slice", label, entryLabel, fieldTag);
			return std::nullopt;
		}

		return std::string(stringBlob + offset, length);
	};

	for (uint32_t i = 0; i < header.entryCount; ++i) {
		const BTLOCEntry& src = entries[i];
		const TextID id{src.textID};

		const std::string entryLabel = describeTextID(outSource, id);

		PluralForms forms;
		bool sliceFailed = false;

		for (size_t cat = 0; cat < 6; ++cat) {
			if (src.formOffset[cat] == BTLOC_ABSENT_OFFSET)
				continue;

			auto slice = sliceOf(entryLabel, "(form)", src.formOffset[cat], src.formLength[cat]);
			if (!slice) {
				sliceFailed = true;
				break;
			}

			forms.try_emplace(static_cast<PluralCategory>(cat), std::move(*slice));
		}

		if (sliceFailed)
			return LoadStatus::InvalidPackFile;

		if (forms.empty()) {
			BT_ERROR("LocalizationManager: pack '{}' entry '{}' defines no forms", label, entryLabel);
			return LoadStatus::InvalidEntry;
		}

		std::optional<std::string> context;
		if (src.contextOffset == BTLOC_ABSENT_OFFSET) {
			context = std::string{};
		} else {
			context = sliceOf(entryLabel, "(context)", src.contextOffset, src.contextLength);
			if (!context)
				return LoadStatus::InvalidPackFile;
		}

		Entry entry;
		entry.context = std::move(*context);

		// Collapse other-only entry to simple text.
		if (forms.size() == 1 && forms.contains(PluralCategory::Other)) {
			entry.text = std::move(forms.at(PluralCategory::Other));
		} else {
			entry.text = std::move(forms);
		}

		auto [_, inserted] = outSource.entries.try_emplace(id, std::move(entry));
		if (!inserted)
			BT_WARN("LocalizationManager: pack '{}' contains a duplicate TextID '{}'", label, entryLabel);
	}

	size_t localeLen = 0;
	while (localeLen < sizeof(header.localeCode) && header.localeCode[localeLen] != '\0')
		++localeLen;

	outSource.localeCode.assign(header.localeCode, localeLen);

	return LoadStatus::Success;
}

LoadStatus LocalizationManager::parseLocaleFile(fs::path& path, Source& outSource) {
	std::error_code ec;

	if (!fs::exists(path, ec)) {
		BT_ERROR("LocalizationManager: locale file '{}' not found", path.string());
		return LoadStatus::FileNotFound;
	}

	std::ifstream f(path);
	nlohmann::json data = nlohmann::json::parse(f, nullptr, true, true, true);

	return parseLocaleJSON(data, path.string(), outSource);
}

std::string_view LocalizationManager::getText(TextID id) const {
	return resolveTemplate(id, std::nullopt);
}

std::string_view LocalizationManager::getText(std::string_view id) const {
	return getText(makeTextID(id));
}

std::string_view LocalizationManager::resolveTemplate(TextID id, std::optional<I64> count) const {
	for (const auto& handle : priorityOrder) {
		const auto& source = sources.at(handle);

		const auto entryIt = source.entries.find(id);
		if (entryIt == source.entries.end())
			continue;

		const Entry& entry = entryIt->second;

		if (const auto* plain = std::get_if<std::string>(&entry.text))
			return *plain;

		const auto& forms = std::get<PluralForms>(entry.text);

		PluralCategory category = PluralCategory::Other;
		if (count.has_value()) {
			category = selectPluralCategory(source.localeCode, *count);
		} else {
			BT_WARN("LocalizationManager: '{}' is pluralized but no count was supplied (use format() with pluralArg()); defaulting to 'other'", describeTextID(source, id));
		}

		if (const auto formIt = forms.find(category); formIt != forms.end())
			return formIt->second;

		// Fall back to Other
		if (category != PluralCategory::Other) {
			if (const auto otherIt = forms.find(PluralCategory::Other); otherIt != forms.end())
				return otherIt->second;
		}

		// Other undefined, use first available form.
		if (!forms.empty())
			return forms.begin()->second;

		return {};
	}

	return {};
}

LocalizedString LocalizationManager::formatTemplate(std::string_view tmpl, std::span<const FormatArg> args) {
	LocalizedString out;
	size_t i = 0;

	while (i < tmpl.size()) {
		// Emit the next run of plain text up to the next brace
		// rather than appending character by character.
		const size_t brace = tmpl.find_first_of("{}", i);
		if (brace == std::string_view::npos) {
			out.append(tmpl.substr(i));
			break;
		}

		if (brace > i)
			out.append(tmpl.substr(i, brace - i));

		const char braceChar = tmpl[brace];

		// "{{" / "}}" are literal escaped braces, matching std::format.
		if (brace + 1 < tmpl.size() && tmpl[brace + 1] == braceChar) {
			out.append(std::string_view(&braceChar, 1));
			i = brace + 2;
			continue;
		}

		if (braceChar == '}') {
			// Stray unescaped '}' with no opening '{'
			BT_WARN("LocalizationManager::format: stray '}}' in template '{}'", tmpl);
			out.append("}");
			i = brace + 1;
			continue;
		}

		const size_t close = tmpl.find('}', brace + 1);
		if (close == std::string_view::npos) {
			BT_WARN("LocalizationManager::format: unterminated '{{' in template '{}'", tmpl);
			out.append(tmpl.substr(brace));
			break;
		}

		const std::string_view inner = tmpl.substr(brace + 1, close - brace - 1);
		std::string_view name = inner;
		std::string_view spec;

		if (const auto colon = inner.find(':'); colon != std::string_view::npos) {
			name = inner.substr(0, colon);
			spec = inner.substr(colon + 1);
		}

		const auto argIt = std::find_if(args.begin(), args.end(),
			[name](const FormatArg& a) { return a.name == name; });

		if (argIt != args.end()) {
			out.append(argIt->formatFn(argIt->valuePtr, spec));
		} else {
			BT_WARN("LocalizationManager::format: no value supplied for '{{{}}}' in template '{}'", name, tmpl);
			out.append(tmpl.substr(brace, close - brace + 1)); // literal "{name}" / "{name:spec}"
		}

		i = close + 1;
	}

	return out;
}


} // namespace Blackthorn::Localization