#pragma once

#include <string_view>

#include "Core/Types/Numeric.h"

namespace Blackthorn::Localization {

/**
 * @brief CLDR-style plural categories. Not every language uses every
 *        category (English only ever produces One/Other).
 */
enum class PluralCategory : U8 {
	Zero,
	One,
	Two,
	Few,
	Many,
	Other
};

/**
 * @brief Selects which plural category a count falls into, for a locale.
 *
 * @param localeCode The resolving source's locale code, e.g. "en-US".
 * @param count The value driving pluralization (e.g. an item count).
 */
[[nodiscard]] PluralCategory selectPluralCategory(std::string_view localeCode, I64 count);

} // namespace Blackthorn::Localization
