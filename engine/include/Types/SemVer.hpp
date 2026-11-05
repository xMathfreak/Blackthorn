#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/**
 * @file SemVer.h
 * @brief Header-only Semantic Versioning 2.0.0 values and npm-style ranges.
 */

namespace Blackthorn::Assets {

namespace Detail {

inline bool isDigit(char c) noexcept {
	return c >= '0' && c <= '9';
}

inline bool isIdentifierChar(char c) noexcept {
	return isDigit(c)
		|| (c >= 'a' && c <= 'z')
		|| (c >= 'A' && c <= 'Z')
		|| c == '-';
}

inline bool allDigits(std::string_view s) noexcept {
	if (s.empty())
		return false;

	for (const char c : s)
		if (!isDigit(c))
			return false;

	return true;
}

/**
 * @brief Parses a numeric component with no sign, no leading zeros
 * (a lone "0" is fine) and a value that fits in 32 bits.
 */
inline std::optional<uint32_t> parseNumeric(std::string_view s) noexcept {
	if (s.empty() || s.size() > 10)
		return std::nullopt;

	if (s.size() > 1 && s.front() == '0')
		return std::nullopt;

	uint64_t value = 0;
	for (const char c : s) {
		if (!isDigit(c))
			return std::nullopt;

		value = value * 10 + static_cast<uint64_t>(c - '0');
	}

	if (value > 0xFFFFFFFFull)
		return std::nullopt;

	return static_cast<uint32_t>(value);
}

/**
 * @brief Splits @p s on @p sep, keeping empty pieces.
 */
inline std::vector<std::string_view> split(std::string_view s, char sep) {
	std::vector<std::string_view> out;

	size_t start = 0;
	for (;;) {
		const size_t pos = s.find(sep, start);
		if (pos == std::string_view::npos) {
			out.push_back(s.substr(start));
			break;
		}

		out.push_back(s.substr(start, pos - start));
		start = pos + 1;
	}

	return out;
}

/**
 * @brief Parses dot-separated prerelease identifiers into @p out.
 *
 * Identifiers must be non-empty, contain only [0-9A-Za-z-], and numeric
 * identifiers must not have leading zeros.
 */
inline bool parsePrerelease(std::string_view s, std::vector<std::string>& out) {
	if (s.empty())
		return false;

	for (const std::string_view id : split(s, '.')) {
		if (id.empty())
			return false;

		for (const char c : id)
			if (!isIdentifierChar(c))
				return false;

		if (allDigits(id) && id.size() > 1 && id.front() == '0')
			return false;

		out.emplace_back(id);
	}

	return true;
}

/// Compares two numeric identifiers (no leading zeros) of any length.
inline int compareNumericIdentifiers(const std::string& a, const std::string& b) noexcept {
	if (a.size() != b.size())
		return a.size() < b.size() ? -1 : 1;

	const int c = a.compare(b);
	return c < 0 ? -1 : (c > 0 ? 1 : 0);
}

} // namespace Detail

/**
 * @brief A Semantic Versioning 2.0.0 version: major.minor.patch[-prerelease][+build].
 *
 * Parsing is strict: no leading "v", no leading zeros, exactly three numeric
 * components. Build metadata is preserved but ignored by @c compare().
 */
struct SemVer {
	uint32_t major = 0;
	uint32_t minor = 0;
	uint32_t patch = 0;

	/// Dot-separated prerelease identifiers (empty for a release version).
	std::vector<std::string> prerelease;

	/// Build metadata (empty if none). Ignored for precedence.
	std::string build;

	/**
	 * @brief Parses a strict SemVer 2.0.0 string.
	 * @return The version, or std::nullopt if @p text is not valid.
	 */
	static std::optional<SemVer> parse(std::string_view text) {
		SemVer v;

		const size_t plus = text.find('+');
		if (plus != std::string_view::npos) {
			const std::string_view meta = text.substr(plus + 1);
			if (meta.empty())
				return std::nullopt;

			for (const std::string_view id : Detail::split(meta, '.')) {
				if (id.empty())
					return std::nullopt;

				for (const char c : id)
					if (!Detail::isIdentifierChar(c))
						return std::nullopt;
			}

			v.build = std::string(meta);
			text = text.substr(0, plus);
		}

		const size_t dash = text.find('-');
		if (dash != std::string_view::npos) {
			if (!Detail::parsePrerelease(text.substr(dash + 1), v.prerelease))
				return std::nullopt;

			text = text.substr(0, dash);
		}

		const std::vector<std::string_view> parts = Detail::split(text, '.');
		if (parts.size() != 3)
			return std::nullopt;

		const auto maj = Detail::parseNumeric(parts[0]);
		const auto min = Detail::parseNumeric(parts[1]);
		const auto pat = Detail::parseNumeric(parts[2]);

		if (!maj || !min || !pat)
			return std::nullopt;

		v.major = *maj;
		v.minor = *min;
		v.patch = *pat;

		return v;
	}

	/** @brief Canonical string form, including prerelease and build metadata. */
	std::string toString() const {
		std::string s = std::to_string(major) + "." + std::to_string(minor)
			+ "." + std::to_string(patch);

		for (size_t i = 0; i < prerelease.size(); ++i)
			s += (i == 0 ? "-" : ".") + prerelease[i];

		if (!build.empty())
			s += "+" + build;

		return s;
	}

	bool isPrerelease() const noexcept {
		return !prerelease.empty();
	}

	/**
	 * @brief Three-way comparison by SemVer 2.0.0 precedence.
	 * @return Negative if *this < other, 0 if equal precedence, positive if greater.
	 */
	int compare(const SemVer& other) const noexcept {
		if (major != other.major)
			return major < other.major ? -1 : 1;

		if (minor != other.minor)
			return minor < other.minor ? -1 : 1;

		if (patch != other.patch)
			return patch < other.patch ? -1 : 1;

		if (prerelease.empty() != other.prerelease.empty())
			return prerelease.empty() ? 1 : -1;

		const size_t n = std::min(prerelease.size(), other.prerelease.size());
		for (size_t i = 0; i < n; ++i) {
			const std::string& a = prerelease[i];
			const std::string& b = other.prerelease[i];

			const bool aNum = Detail::allDigits(a);
			const bool bNum = Detail::allDigits(b);

			if (aNum && bNum) {
				const int c = Detail::compareNumericIdentifiers(a, b);
				if (c != 0)
					return c;
			} else if (aNum != bNum) {
				return aNum ? -1 : 1;
			} else {
				const int c = a.compare(b);
				if (c != 0)
					return c < 0 ? -1 : 1;
			}
		}

		if (prerelease.size() != other.prerelease.size())
			return prerelease.size() < other.prerelease.size() ? -1 : 1;

		return 0;
	}
};

inline bool operator==(const SemVer& a, const SemVer& b) noexcept { return a.compare(b) == 0; }
inline bool operator!=(const SemVer& a, const SemVer& b) noexcept { return a.compare(b) != 0; }
inline bool operator<(const SemVer& a, const SemVer& b) noexcept { return a.compare(b) < 0; }
inline bool operator>(const SemVer& a, const SemVer& b) noexcept { return a.compare(b) > 0; }
inline bool operator<=(const SemVer& a, const SemVer& b) noexcept { return a.compare(b) <= 0; }
inline bool operator>=(const SemVer& a, const SemVer& b) noexcept { return a.compare(b) >= 0; }

/**
 * @brief An npm-style version range, e.g. "^1.2.3", "~1.2", ">=1.0.0 <2.0.0",
 * "1.2.x", "1.x || >=3.0.0", or "*".
 *
 * @par Supported syntax
 * - Exact: "1.2.3", "=1.2.3"
 * - Caret: "^1.2.3" (compatible with), "^0.2.3" -> <0.3.0, "^0.0.3" -> <0.0.4
 * - Tilde: "~1.2.3" -> <1.3.0, "~1" -> <2.0.0
 * - Comparators: ">", ">=", "<", "<=" (optionally with a space after the operator)
 * - Partial / wildcard: "1", "1.2", "1.x", "1.2.*", "*", "x"
 * - AND: comparators separated by whitespace or commas
 * - OR: alternatives separated by "||"
 * - An empty range means "any version".
 *
 * Hyphen ranges ("1.2.3 - 2.3.4") are not supported and fail to parse.
 *
 * @par Prerelease matching
 * Like npm, a prerelease version only satisfies a range if some comparator in
 * the satisfied comparator set names a prerelease of the same
 * major.minor.patch. So "^1.2.3" does not match "1.3.0-beta", but
 * "^1.2.3-beta.1" matches "1.2.3-beta.4".
 */
class SemVerRange {
public:
	/** @brief A range that every release version satisfies. */
	static SemVerRange any() {
		SemVerRange r;
		r.alternatives.emplace_back();
		return r;
	}

	/**
	 * @brief Parses @p text into a range.
	 * @return The range, or std::nullopt if @p text is malformed.
	 */
	static std::optional<SemVerRange> parse(std::string_view text) {
		SemVerRange range;

		const std::string_view trimmed = trim(text);
		if (trimmed.empty()) {
			range.alternatives.emplace_back();
			return range;
		}

		std::vector<std::string_view> alts;
		size_t start = 0;
		for (;;) {
			const size_t pos = trimmed.find("||", start);
			if (pos == std::string_view::npos) {
				alts.push_back(trimmed.substr(start));
				break;
			}

			alts.push_back(trimmed.substr(start, pos - start));
			start = pos + 2;
		}

		for (const std::string_view alt : alts) {
			const std::string_view a = trim(alt);

			if (a.empty() && alts.size() > 1)
				return std::nullopt;

			ComparatorSet set;
			if (!parseComparatorSet(a, set))
				return std::nullopt;

			range.alternatives.push_back(std::move(set));
		}

		return range;
	}

	/** @brief Returns true if @p v satisfies this range. */
	bool satisfiedBy(const SemVer& v) const {
		for (const ComparatorSet& set : alternatives)
			if (setSatisfiedBy(set, v))
				return true;

		return false;
	}

private:
	enum class Op : uint8_t { Eq, Lt, Lte, Gt, Gte };

	struct Comparator {
		Op op;
		SemVer version;
	};

	using ComparatorSet = std::vector<Comparator>;

	/// A version with only its leading numeric components specified.
	struct Partial {
		uint32_t parts[3] = { 0, 0, 0 };
		int count = 0; ///< Number of leading concrete components (0-3).
		std::vector<std::string> prerelease;
	};

	std::vector<ComparatorSet> alternatives;

	static std::string_view trim(std::string_view s) noexcept {
		while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
			s.remove_prefix(1);

		while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
			s.remove_suffix(1);

		return s;
	}

	static bool isOperatorChar(char c) noexcept {
		return c == '<' || c == '>' || c == '=' || c == '^' || c == '~';
	}

	static bool test(const Comparator& c, const SemVer& v) {
		const int r = v.compare(c.version);

		switch (c.op) {
			case Op::Eq:  return r == 0;
			case Op::Lt:  return r < 0;
			case Op::Lte: return r <= 0;
			case Op::Gt:  return r > 0;
			case Op::Gte: return r >= 0;
		}

		return false;
	}

	static bool setSatisfiedBy(const ComparatorSet& set, const SemVer& v) {
		for (const Comparator& c : set)
			if (!test(c, v))
				return false;

		if (!v.isPrerelease())
			return true;

		for (const Comparator& c : set) {
			if (c.version.isPrerelease()
				&& c.version.major == v.major
				&& c.version.minor == v.minor
				&& c.version.patch == v.patch)
			{
				return true;
			}
		}

		return false;
	}

	static std::optional<Partial> parsePartial(std::string_view s) {
		if (!s.empty() && (s.front() == 'v' || s.front() == 'V'))
			s.remove_prefix(1);

		const size_t plus = s.find('+');
		if (plus != std::string_view::npos)
			s = s.substr(0, plus);

		std::string_view pre;
		bool hasPre = false;

		const size_t dash = s.find('-');
		if (dash != std::string_view::npos) {
			pre = s.substr(dash + 1);
			s = s.substr(0, dash);
			hasPre = true;
		}

		const std::vector<std::string_view> comps = Detail::split(s, '.');
		if (comps.empty() || comps.size() > 3)
			return std::nullopt;

		Partial p;
		bool wildcard = false;

		for (size_t i = 0; i < comps.size(); ++i) {
			const std::string_view c = comps[i];

			if (c == "x" || c == "X" || c == "*") {
				wildcard = true;
				continue;
			}

			if (wildcard)
				return std::nullopt;

			const auto n = Detail::parseNumeric(c);
			if (!n)
				return std::nullopt;

			p.parts[i] = *n;
			p.count = static_cast<int>(i) + 1;
		}

		if (hasPre) {
			if (p.count != 3 || !Detail::parsePrerelease(pre, p.prerelease))
				return std::nullopt;
		}

		return p;
	}

	static SemVer make(uint32_t maj, uint32_t min, uint32_t pat,
		const std::vector<std::string>& pre = {})
	{
		SemVer v;
		v.major = maj;
		v.minor = min;
		v.patch = pat;
		v.prerelease = pre;
		return v;
	}

	/// Increments @p v, failing (returning false) instead of wrapping.
	static bool bump(uint32_t& v) noexcept {
		if (v == 0xFFFFFFFFu)
			return false;

		++v;
		return true;
	}

	/**
	 * @brief Expands one "<op><partial>" token into concrete comparators.
	 * @param op Operator text: "", "=", "^", "~", "~>", ">", ">=", "<", "<=".
	 */
	static bool desugar(std::string_view op, const Partial& p, ComparatorSet& out) {
		const uint32_t M = p.parts[0];
		const uint32_t m = p.parts[1];
		const uint32_t pt = p.parts[2];
		const int n = p.count;

		// Upper bound helpers ("<" comparators on the next major/minor/patch).
		const auto nextMajor = [&](uint32_t& outM) { outM = M; return bump(outM); };
		const auto nextMinor = [&](uint32_t& outMin) { outMin = m; return bump(outMin); };

		uint32_t hi = 0;

		if (op.empty() || op == "=") {
			if (n == 0)
				return true;

			if (n == 3) {
				out.push_back({ Op::Eq, make(M, m, pt, p.prerelease) });
				return true;
			}

			if (n == 2) {
				if (!nextMinor(hi))
					return false;

				out.push_back({ Op::Gte, make(M, m, 0) });
				out.push_back({ Op::Lt, make(M, hi, 0) });
				return true;
			}

			if (!nextMajor(hi))
				return false;

			out.push_back({ Op::Gte, make(M, 0, 0) });
			out.push_back({ Op::Lt, make(hi, 0, 0) });
			return true;
		}

		if (op == "^") {
			if (n == 0)
				return true;

			if (n == 1) {
				if (!nextMajor(hi))
					return false;

				out.push_back({ Op::Gte, make(M, 0, 0) });
				out.push_back({ Op::Lt, make(hi, 0, 0) });
				return true;
			}

			if (n == 2) {
				out.push_back({ Op::Gte, make(M, m, 0) });

				if (M > 0) {
					if (!nextMajor(hi))
						return false;

					out.push_back({ Op::Lt, make(hi, 0, 0) });
				} else {
					if (!nextMinor(hi))
						return false;

					out.push_back({ Op::Lt, make(0, hi, 0) });
				}

				return true;
			}

			out.push_back({ Op::Gte, make(M, m, pt, p.prerelease) });

			if (M > 0) {
				if (!nextMajor(hi))
					return false;

				out.push_back({ Op::Lt, make(hi, 0, 0) });
			} else if (m > 0) {
				if (!nextMinor(hi))
					return false;

				out.push_back({ Op::Lt, make(0, hi, 0) });
			} else {
				uint32_t nextPatch = pt;
				if (!bump(nextPatch))
					return false;

				out.push_back({ Op::Lt, make(0, 0, nextPatch) });
			}

			return true;
		}

		if (op == "~" || op == "~>") {
			if (n == 0)
				return true;

			if (n == 1) {
				if (!nextMajor(hi))
					return false;

				out.push_back({ Op::Gte, make(M, 0, 0) });
				out.push_back({ Op::Lt, make(hi, 0, 0) });
				return true;
			}

			if (!nextMinor(hi))
				return false;

			out.push_back({
				Op::Gte,
				n == 3 ? make(M, m, pt, p.prerelease) : make(M, m, 0)
			});
			out.push_back({ Op::Lt, make(M, hi, 0) });
			return true;
		}

		if (op == ">=") {
			if (n == 0)
				return true;

			out.push_back({
				Op::Gte,
				n == 3 ? make(M, m, pt, p.prerelease) : make(M, n == 2 ? m : 0, 0)
			});
			return true;
		}

		if (op == ">") {
			if (n == 0) {
				out.push_back({ Op::Lt, make(0, 0, 0) });
				return true;
			}

			if (n == 3) {
				out.push_back({ Op::Gt, make(M, m, pt, p.prerelease) });
				return true;
			}

			if (n == 2) {
				if (!nextMinor(hi))
					return false;

				out.push_back({ Op::Gte, make(M, hi, 0) });
				return true;
			}

			if (!nextMajor(hi))
				return false;

			out.push_back({ Op::Gte, make(hi, 0, 0) });
			return true;
		}

		if (op == "<=") {
			if (n == 0)
				return true;

			if (n == 3) {
				out.push_back({ Op::Lte, make(M, m, pt, p.prerelease) });
				return true;
			}

			if (n == 2) {
				if (!nextMinor(hi))
					return false;

				out.push_back({ Op::Lt, make(M, hi, 0) });
				return true;
			}

			if (!nextMajor(hi))
				return false;

			out.push_back({ Op::Lt, make(hi, 0, 0) });
			return true;
		}

		if (op == "<") {
			if (n == 0) {
				out.push_back({ Op::Lt, make(0, 0, 0) });
				return true;
			}

			out.push_back({
				Op::Lt,
				n == 3 ? make(M, m, pt, p.prerelease) : make(M, n == 2 ? m : 0, 0)
			});
			return true;
		}

		return false;
	}

	/// Parses one alternative (comparators joined by whitespace/commas).
	static bool parseComparatorSet(std::string_view text, ComparatorSet& out) {
		std::vector<std::string> tokens;

		size_t i = 0;
		while (i < text.size()) {
			while (i < text.size()
				&& (text[i] == ' ' || text[i] == '\t' || text[i] == ','))
			{
				++i;
			}

			const size_t begin = i;
			while (i < text.size()
				&& text[i] != ' ' && text[i] != '\t' && text[i] != ',')
			{
				++i;
			}

			if (i > begin)
				tokens.emplace_back(text.substr(begin, i - begin));
		}

		for (size_t t = 0; t < tokens.size(); ++t) {
			std::string token = tokens[t];

			// An operator separated from its version by whitespace: ">= 1.2.3".
			const bool onlyOperators = std::all_of(
				token.begin(), token.end(),
				[](char c) { return isOperatorChar(c); }
			);

			if (onlyOperators) {
				if (t + 1 >= tokens.size())
					return false;

				token += tokens[++t];
			}

			size_t opLen = 0;
			while (opLen < token.size() && isOperatorChar(token[opLen]))
				++opLen;

			const std::string op = token.substr(0, opLen);
			const bool knownOp = op.empty() || op == "=" || op == "^" || op == "~"
				|| op == "~>" || op == ">" || op == ">=" || op == "<" || op == "<=";

			if (!knownOp)
				return false;

			const auto partial = parsePartial(std::string_view(token).substr(opLen));
			if (!partial)
				return false;

			if (!desugar(op, *partial, out))
				return false;
		}

		return true;
	}
};

} // namespace Blackthorn::Assets
