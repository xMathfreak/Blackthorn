#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Localization/PluralRules.h"

namespace BTLocC {

/**
 * @brief One parsed locale JSON entry, before hashing/encoding.
 *
 * Exactly one of plainText or forms is populated, never both and never
 * neither (LocParser enforces this while parsing). context is independent
 * of which one is set.
 */
struct LocEntryValue {
	std::string context;
	std::optional<std::string> plainText;
	std::unordered_map<Blackthorn::Localization::PluralCategory, std::string> forms;
};

/**
 * @brief A fully parsed locale JSON file.
 * @note entries is a vector of pairs, not a map, to preserve the JSON's key
 *       order. That order becomes the generated TextID header's constant
 *       order, so re-running btlocc without changing the JSON produces a
 *       byte-identical header (stable diffs).
 */
struct LocFile {
	std::string localeCode;
	std::string sourcePath; ///< For error messages; not written to the .btloc file.
	std::vector<std::pair<std::string, LocEntryValue>> entries;
};

} // namespace BTLocC
