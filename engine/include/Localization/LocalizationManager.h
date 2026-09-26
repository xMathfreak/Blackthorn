#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "Core/Export.h"
#include "Core/Types/Numeric.h"
#include "Localization/FormatArg.h"
#include "Localization/LocalizationSource.h"
#include "Localization/LocalizedString.h"

namespace Blackthorn::Localization {

enum class LoadStatus : U8 {
	Success,
	FileNotFound,
	InvalidJSON,
	MissingLocaleCode,
	MissingEntries,
	LocaleMismatch,
	InvalidEntry,
	InvalidPackFile ///< A .btloc file failed header/version/decompression checks (loadFromPack).
};

struct LoadResult {
	std::optional<SourceHandle> handle;
	LoadStatus status;
};

class BLACKTHORN_API LocalizationManager {
public:
	LocalizationManager();
	~LocalizationManager();

	void setLocaleCode(std::string_view lc);
	[[nodiscard]] std::string_view getLocaleCode() const noexcept;

	// Higher priority wins when multiple sources define the same TextID; ties
	// are broken by load order (earlier-loaded source wins). See Source::priority.
	LoadResult loadFromFile(std::filesystem::path path, I32 priority = 0);

	/**
	 * @brief Loads a compiled .btloc file (see LocFormat.h), produced by the
	 *        btlocc localization compiler, instead of parsing JSON.
	 *
	 * @param path Path to a standalone .btloc file.
	 * @param priority Same meaning as loadFromFile's.
	 */
	LoadResult loadFromPack(std::filesystem::path path, I32 priority = 0);

	/**
	 * @brief Parses locale JSON already resident in memory.
	 *
	 * @warning This mutates manager state and is not thread safe. Only call from the main thread.
	 *
	 * @param jsonByte Raw JSON file bytes.
	 * @param priority Same meaning as loadFromFile's.
	 * @param label    Used only in diagnosis since there's no std::filesystem::path to log.
	 */
	LoadResult loadFromMemory(std::span<const U8> jsonBytes, I32 priority, std::string_view label);

	/**
	 * @brief Byte-buffer counterpart to loadFromPack, for a `.btloc` blob
	 * already resolved into memory.
	 *
	 * @warning Same thread affinity restriction as loadFromMemory().
	 */
	LoadResult loadFromPackBytes(std::span<const U8> btlocBytes, I32 priority, std::string_view label);

	/**
	 * @brief Removes a previously loaded source, freeing its entries and
	 * pulling it out of priority resolution.
	 *
	 * @warning Not thread-safe like every other mutating call on this
	 * class. Only call it from the main thread.
	 *
	 * @warning Invalidates every std::string_view previously returned by
	 * getText()/format() for entries that lived only in this source. Do not
	 * hold on to resolved text across a frame boundary where an unload could
	 * happen, re-resolve per use instead.
	 *
	 * @param handle A handle from any of this class's load methods.
	 *
	 * @return true if @P handle was loaded and is now unloaded; false if it
	 * was already unloaded or never valid. Safe to call on a stale handle.
	*/
	bool unloadSource(SourceHandle handle);

	/**
	 * @brief Resolve a TextID against all loaded sources, honoring priority.
	 * @return The text for the highest-priority source that defines id, or an
	 *         empty view if no loaded source defines it.
	 *
	 * @note If the entry is pluralized, no count is available here to pick a
	 * 		 form, so this always resolves to PluralForm::Other with a
	 * 		 warning. Prefer format() with pluralArg() for those entries.
	 */
	[[nodiscard]] std::string_view getText(TextID id) const;

	/**
	 * @brief Convenience overload: hashes id via makeTextID() and resolves it.
	 *        Prefer the TextID overload on hot paths once callers have IDs
	 *        precomputed, since this overload re-hashes the string on every call.
	 */
	[[nodiscard]] std::string_view getText(std::string_view id) const;

	/**
	 * @brief Resolves id's template and substitutes named variables into it,
	 *        e.g. format(id, arg("count", 5)) for a template of
	 *        "You found {count} potions.".
	 *
	 * @details Each {name} placeholder is looked up by name against args, in
	 *          the order given, and formatted via std::format/std::vformat
	 *          so any type with a std::formatter specialization works, not
	 *          just numbers and strings. A placeholder may also carry a
	 *          std::format-style spec after a colon, e.g. "{count:.2f}".
	 *          "{{" and "}}" are literal braces, matching std::format's escape
	 *          convention.
	 *
	 * @note If a placeholder's name has no matching arg, a warning is logged
	 * 		 and the placeholder's literal text (e.g. "{count}") is left in
	 * 		 the output, so a missing translation variable is visible in-game
	 * 		 rather than silently swallowed.
	 *
	 * @note If @p id resloves to a pluralized entry, use the pluralArg()
	 * 		 overload instead. This overload will always resolve such entries
	 * 		 to PluralForm::Other and log a warning.
	 *
	 * @warning args must be constructed inline in the call (e.g. via arg()).
	 */
	template<typename... Args>
	[[nodiscard]] LocalizedString format(TextID id, const NamedArg<Args>&... args) const {
		const std::array<FormatArg, sizeof...(Args)> erased{ makeFormatArg(args)... };
		return formatTemplate(resolveTemplate(id, std::nullopt), erased);
	}

	/// @brief Convenience overload taking a human-readable id; see the
	///        getText(std::string_view) overload's note on re-hashing cost.
	template<typename... Args>
	[[nodiscard]] LocalizedString format(std::string_view id, const NamedArg<Args>&... args) const {
		return format(makeTextID(id), args...);
	}

	/**
	 * @brief Overload for pluralized entries.
	 *
	 * @p pluralSelector selects the plural category from the winning source's
	 * locale and is available for substitution, e.g `format(id, pluralArg("count", n))` for
	 * a template pair of `{"one": "You found one potion", "other": "You found {count} potions"}`.
	 *
	 * @note If @p id does not resolve to a pluralized entry, @p pluralSelector
	 * is simply used as a regular substitution value and its selection role is
	 * a no-op.
	 */
	template<typename C, typename... Args>
	[[nodiscard]] LocalizedString format(TextID id, const PluralArg<C>& pluralSelector, const NamedArg<Args>&... args) const {
		const std::array<FormatArg, sizeof...(Args) + 1> erased{
			makeFormatArg(pluralSelector),
			makeFormatArg(args)...
		};
		return formatTemplate(resolveTemplate(id, static_cast<I64>(pluralSelector.value)), erased);
	}

	/// @brief Convenience overload taking a human-readable id; see the
	///        getText(std::string_view) overload's note on re-hashing cost.
	template<typename C, typename... Args>
	[[nodiscard]] LocalizedString format(std::string_view id, const PluralArg<C>& pluralSelector, const NamedArg<Args>&... args) const {
		return format(makeTextID(id), pluralSelector, args...);
	}

private:
	LoadStatus parseLocaleFile(std::filesystem::path& path, Source& outSource);

	// Reads and validates a .btloc file's header/entries/string blob into
	// outSource. Mirrors parseLocaleFile's role for the JSON path.
	LoadStatus parsePackFile(std::filesystem::path& path, Source& outSource);

	LoadStatus parseLocaleJSON(const nlohmann::json& data, std::string_view label, Source& outSource);
	LoadStatus parsePackBytes(std::span<const U8> bytes, std::string_view label, Source& outSource);

	// Inserts handle into priorityOrder at the position matching priority,
	// keeping the vector sorted descending by priority (ties keep existing/
	// earlier-loaded entries ahead of the newly inserted one).
	void insertIntoPriorityOrder(SourceHandle handle, I32 priority);

	// Walks tmpl, substituting each {name} (or {name:spec}) placeholder with
	// the matching entry in args, formatted via FormatArg::formatFn. Ordinary
	// (non-template) function: the templated format() overloads above only
	// type-erase their arguments into `args` and forward here, so this parsing
	// logic is compiled once rather than once per distinct Args... combination.
	static LocalizedString formatTemplate(std::string_view tmpl, std::span<const FormatArg> args);

	// Resolves id to its template text across all loaded sources (priority
	// order). If the winning entry is pluralized, count selects which form
	// falling back to Other, then to whatever form exists, if the selected
	// category isn't defined in that entry. `count == std::nullopt` means
	// the caller didn't supply a plural selector (getText(), or format() without
	// pluralArg()); that case is logged and treated as Other.
	[[nodiscard]] std::string_view resolveTemplate(TextID id, std::optional<I64> count) const;

	LocaleCode localeCode;
	std::unordered_map<SourceHandle, Source> sources;

	// Loaded source handles, kept sorted descending by Source::priority, so
	// getText() can stop at the first match instead of scanning every
	// loaded source on every call.
	std::vector<SourceHandle> priorityOrder;

	SourceHandle nextHandle{0};
};

} // namespace Blackthorn::Localization