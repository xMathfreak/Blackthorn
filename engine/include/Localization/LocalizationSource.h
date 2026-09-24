#pragma once

#include <string>
#include <unordered_map>
#include <variant>

#include "Core/Types/Numeric.h"
#include "Localization/PluralRules.h"
#include "Localization/TextID.h"

namespace Blackthorn::Localization {

using LocaleCode = std::string;

struct SourceHandle {
	U32 value = 0;

	constexpr bool operator<=>(const SourceHandle&) const = default;

	constexpr SourceHandle& operator++() {
		++value;
		return *this;
	}

	constexpr SourceHandle operator++(int) {
		SourceHandle old = *this;
		++value;
		return old;
	}
};

} // namespace Blackthorn::Localization

namespace std {

template<>
struct hash<Blackthorn::Localization::SourceHandle> {
	size_t operator()(const Blackthorn::Localization::SourceHandle& t) const noexcept {
		return static_cast<size_t>(t.value);
	}
};

} // namespace std

namespace Blackthorn::Localization {

/**
 * @brief Pluralized text forms for a localization entry, keyed by plural
 * 		  category ("one", "other", etc).
 *
 * @details A source does not need to provide every category. When the
 * requested category is missing, resolution falls back to "other", then
 * to any available form.
 *
 * @see LocalizationManager::resolveTemplate().
 */
using PluralForms = std::unordered_map<PluralCategory, std::string>;

/**
 * @brief A single loaded localization entry.
 *
 * @details An entry contains the translator-facing context, if any
 * and either a single text template or a set of pluralized templates.
 * For a plain JSON string, text contains the template directly:
 *
 * - "Your inventory is empty"
 *
 * For a pluralized entry, text contains a PluralForms map ("one", "other",
 * etc):
 *
 * - PluralCategory::Zero - "You have no coins"
 * - PluralCategory::One - "You have one coin"
 * - PluralCategoru::Other - "You have many coins"
 *
 * The appropriate form is selected by LocalizationManager::resolveTemplate().
 *
 * The context field contains optional translator-facing notes from the JSON and
 * is independent from the selected text form. It is currently retained for
 * diagnostics and editor facing tooling.
 */
struct Entry {
	std::string context;
	std::variant<std::string, PluralForms> text;
};

/**
 * @brief A loaded localization file.
 *
 * @details A Source own the entries and associated metadata for a loaded
 * 			localization resource. Entries are keyed by their hashed TextID.
 * 			When multiple sources provide the same TextID, @ref priority
 * 			determines which source supplies the resolved entry. Higher
 * 			priority wins; sources with equal priority are resolved by load
 * 			order with the earliest loaded source winning.
 */
struct Source {
	SourceHandle handle;
	LocaleCode localeCode;

	/**
	 * @brief Resolution priority for entries provided by this source.
	 *
	 * @details When multiple loaded sources define the same TextID, the source
	 *          with the higher priority wins. If priorities are equal, the
	 *          earlier-loaded source wins.
	 */
	I32 priority = 0;



	/**
	 * @brief Localization entries keyed by their hashed TextID.
	 *
	 * @details The map owns all text and context strings loaded from the
	 *          localization file. Consequently, string_views returned by
	 *          getText()/get() may refer directly to this owned storage and
	 *          remain valid for as long as the referenced Entry remains in the
	 *          map.
	 *
	 *          Rehashing does not invalidate references or pointers to existing
	 *          unordered_map elements, so it does not invalidate such views.
	 */
	std::unordered_map<TextID, Entry> entries;

	/**
	 * @brief Maps TextIDs to their original human-readable localization keys.
	 *
	 * @details Used only for diagnostics, so errors and warnings can identify
	 *          an entry by its key rather than by its raw TextID.
	 *
	 *          When loading from JSON, this table is always populated because
	 *          the original key is available during parsing.
	 *
	 *          When loading from a .btloc file, this table is populated only
	 *          when the file contains a debug symbol table. See LocFormat.h.
	 *          btlocc includes symbols by default; --no-symbols omits them.
	 *
	 *          Consequently, diagnostic lookups must tolerate a missing symbol
	 *          and fall back to displaying the raw TextID.
	 */
	std::unordered_map<TextID, std::string> debugSymbols;
};

} // namespace Blackthorn::Localization

