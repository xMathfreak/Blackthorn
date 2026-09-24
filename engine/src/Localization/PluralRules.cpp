#include "Localization/PluralRules.h"

namespace Blackthorn::Localization {

namespace {

I64 mod(I64 val, I64 div) {
	return val % div;
}

PluralCategory defaultRule(I64 count) {
	return count == 1 ? PluralCategory::One : PluralCategory::Other;
}

PluralCategory frenchRule(I64 count) {
	// fr: one for 0 and 1.
	return count == 0 || count == 1
		? PluralCategory::One
		: PluralCategory::Other;
}

PluralCategory russianRule(I64 count) {
	// ru: one 1, 21, 31...; few 2-4, 22-24...;
	// many 0, 5-20, 25-30...
	const I64 n = count < 0 ? -count : count;

	if (mod(n, 10) == 1 &&
		mod(n, 100) != 11)
		return PluralCategory::One;

	if (mod(n, 10) >= 2 && mod(n, 10) <= 4 &&
		!(mod(n, 100) >= 12 && mod(n, 100) <= 14))
		return PluralCategory::Few;

	if (mod(n, 10) == 0 ||
		(mod(n, 10) >= 5 && mod(n, 10) <= 9) ||
		(mod(n, 100) >= 11 && mod(n, 100) <= 14))
		return PluralCategory::Many;

	return PluralCategory::Other;
}

PluralCategory ukrainianRule(I64 count) {
	const I64 n = count < 0 ? -count : count;

	if (mod(n, 10) == 1 && mod(n, 100) != 11)
		return PluralCategory::One;

	if (mod(n, 10) >= 2 && mod(n, 10) <= 4 &&
		!(mod(n, 100) >= 12 && mod(n, 100) <= 14))
		return PluralCategory::Few;

	if (mod(n, 10) == 0 ||
		(mod(n, 10) >= 5 && mod(n, 10) <= 9) ||
		(mod(n, 100) >= 11 && mod(n, 100) <= 14))
		return PluralCategory::Many;

	return PluralCategory::Other;
}

PluralCategory polishRule(I64 count) {
	const I64 n = count < 0 ? -count : count;

	if (n == 1)
		return PluralCategory::One;

	if (mod(n, 10) >= 2 && mod(n, 10) <= 4 &&
		!(mod(n, 100) >= 12 && mod(n, 100) <= 14))
		return PluralCategory::Few;

	if (mod(n, 10) == 0 ||
		mod(n, 10) == 1 ||
		(mod(n, 10) >= 5 && mod(n, 10) <= 9) ||
		(mod(n, 100) >= 12 && mod(n, 100) <= 14))
		return PluralCategory::Many;

	return PluralCategory::Other;
}

PluralCategory czechRule(I64 count) {
	if (count == 1)
		return PluralCategory::One;

	if (count >= 2 && count <= 4)
		return PluralCategory::Few;

	return PluralCategory::Other;
}

PluralCategory slovakRule(I64 count) {
	return czechRule(count);
}

PluralCategory slovenianRule(I64 count) {
	const I64 n = count < 0 ? -count : count;

	if (mod(n, 100) == 1)
		return PluralCategory::One;

	if (mod(n, 100) == 2)
		return PluralCategory::Two;

	if (mod(n, 100) == 3 || mod(n, 100) == 4)
		return PluralCategory::Few;

	return PluralCategory::Other;
}

PluralCategory romanianRule(I64 count) {
	const I64 n = count < 0 ? -count : count;

	if (n == 1)
		return PluralCategory::One;

	if (n == 0 || (mod(n, 100) >= 1 && mod(n, 100) <= 19))
		return PluralCategory::Few;

	return PluralCategory::Other;
}

PluralCategory lithuanianRule(I64 count) {
	const I64 n = count < 0 ? -count : count;

	if (mod(n, 10) == 1 &&
		!(mod(n, 100) >= 11 && mod(n, 100) <= 19))
		return PluralCategory::One;

	if (mod(n, 10) >= 2 && mod(n, 10) <= 9 &&
		!(mod(n, 100) >= 11 && mod(n, 100) <= 19))
		return PluralCategory::Few;

	return PluralCategory::Other;
}

PluralCategory latvianRule(I64 count) {
	const I64 n = count < 0 ? -count : count;

	if (mod(n, 10) == 0 ||
		(mod(n, 100) >= 11 && mod(n, 100) <= 19))
		return PluralCategory::Zero;

	if (mod(n, 10) == 1 &&
		mod(n, 100) != 11)
		return PluralCategory::One;

	return PluralCategory::Other;
}

PluralCategory irishRule(I64 count) {
	if (count == 1)
		return PluralCategory::One;

	if (count == 2)
		return PluralCategory::Two;

	if (count >= 3 && count <= 6)
		return PluralCategory::Few;

	if (count >= 7 && count <= 10)
		return PluralCategory::Many;

	return PluralCategory::Other;
}

PluralCategory arabicRule(I64 count) {
	if (count == 0)
		return PluralCategory::Zero;

	if (count == 1)
		return PluralCategory::One;

	if (count == 2)
		return PluralCategory::Two;

	const I64 n = count < 0 ? -count : count;

	if (n % 100 >= 3 && n % 100 <= 10)
		return PluralCategory::Few;

	if (n % 100 >= 11 && n % 100 <= 99)
		return PluralCategory::Many;

	return PluralCategory::Other;
}

PluralCategory hebrewRule(I64 count) {
	if (count == 1)
		return PluralCategory::One;

	if (count == 2)
		return PluralCategory::Two;

	if (count != 0 && mod(count, 10) == 0)
		return PluralCategory::Many;

	return PluralCategory::Other;

}

PluralCategory welshRule(I64 count) {
	switch (count) {
		case 0: return PluralCategory::Zero;
		case 1: return PluralCategory::One;
		case 2: return PluralCategory::Two;
		case 3: return PluralCategory::Few;
		case 6: return PluralCategory::Many;
		default: return PluralCategory::Other;
	}
}


} // namespace

PluralCategory selectPluralCategory(std::string_view localeCode, I64 count) {
	const auto separator = localeCode.find_first_of("-_");
	const auto language = localeCode.substr(0, separator);

	if (language == "ar")
		return arabicRule(count);

	if (language == "cs")
		return czechRule(count);

	if (language == "cy")
		return welshRule(count);

	if (language == "fr")
		return frenchRule(count);

	if (language == "ga")
		return irishRule(count);

	if (language == "he")
		return hebrewRule(count);

	if (language == "lt")
		return lithuanianRule(count);

	if (language == "lv")
		return latvianRule(count);

	if (language == "pl")
		return polishRule(count);

	if (language == "ro")
		return romanianRule(count);

	if (language == "ru")
		return russianRule(count);

	if (language == "sk")
		return slovakRule(count);

	if (language == "sl")
		return slovenianRule(count);

	if (language == "uk")
		return ukrainianRule(count);

	return defaultRule(count);
}

} // namespace Blackthorn::Localization
