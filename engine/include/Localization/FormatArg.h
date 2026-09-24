#pragma once

#include <format>
#include <string>
#include <string_view>

namespace Blackthorn::Localization {

/**
 * @brief A single named value to substitute into a localized template, e.g.
 *        the "count" in "You found {count} potions."
 *
 * @warning Holds value by reference. Only ever create these as temporaries
 *          directly inside a LocalizationManager::format(...) call. They
 *          must not outlive the full expression that constructs them
 *          (the same lifetime rule std::make_format_args imposes on its
 *          arguments).
 */
template<typename T>
struct NamedArg {
	std::string_view name;
	const T& value;
};

/// @brief Convenience constructor so call sites read as arg("count", 5)
///        rather than spelling out NamedArg<int>{"count", 5}.
template<typename T>
constexpr NamedArg<T> arg(std::string_view name, const T& value) {
	return NamedArg<T>{name, value};
}

/**
 * @brief Like NamedArg, but also marks the value that should drive plural
 *        category selection for a pluralized entry (see PluralRules.h and
 *        LocalizationManager::format). The same value is still available
 *        for substitution, e.g. pluralArg("count", 5) both selects the
 *        plural form and fills in "{count}" so callers don't pass count
 *        twice.
 *
 * @warning Same reference-lifetime rule as NamedArg. Construct only as a
 *          temporary directly inside the format(...) call.
 */
template<typename T>
struct PluralArg {
	std::string_view name;
	const T& value;
};

/// @brief Convenience constructor, mirroring arg(). value must be usable as
///        the plural count and convertible to I64 (see selectPluralCategory).
template<typename T>
constexpr PluralArg<T> pluralArg(std::string_view name, const T& value) {
	return PluralArg<T>{name, value};
}

/**
 * @brief Type-erased view of a single NamedArg.
 */
struct FormatArg {
	std::string_view name;
	const void* valuePtr;

	/// @brief Formats *valuePtr (cast back to its real type) via
	///        std::format/std::vformat. spec is the text after ':' in a
	///        placeholder like "{count:.2f}" (empty for a plain "{count}").
	std::string (*formatFn)(const void* valuePtr, std::string_view spec);
};

namespace detail {

	template<typename T>
	std::string formatValue(const void* valuePtr, std::string_view spec) {
		const T& value = *static_cast<const T*>(valuePtr);

		if (spec.empty())
			return std::format("{}", value);

		// The spec comes from a runtime-loaded template, so it can't be
		// validated at compile time the way a std::format_string literal
		// would be.
		const std::string fmtStr = "{:" + std::string(spec) + "}";
		return std::vformat(fmtStr, std::make_format_args(value));
	}

} // namespace detail

template<typename T>
constexpr FormatArg makeFormatArg(const NamedArg<T>& named) {
	return FormatArg{named.name, &named.value, &detail::formatValue<T>};
}

/// @brief Same erasure as the NamedArg overload. Lets format() build one
///        uniform FormatArg array regardless of whether an argument is a
///        plain arg() or the pluralArg() driving category selection.
template<typename T>
constexpr FormatArg makeFormatArg(const PluralArg<T>& named) {
	return FormatArg{named.name, &named.value, &detail::formatValue<T>};
}

} // namespace Blackthorn::Localization
