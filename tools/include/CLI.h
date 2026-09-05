#pragma once

/**
 * @file CLI.h
 * @brief Centralized command-line parsing library shared by Blackthorn's tools.
 *
 * Usage sketch:
 *
 * @code
 * Blackthorn::Tools::Command app{"mytool", "A useful command-line tool"};
 * // or: namespace cli = Blackthorn::Tools; cli::Command app{...};
 *
 * app.option("verbose", 'v')
 *     .flag()
 *     .help("Enable verbose output");
 *
 * app.option("jobs", 'j')
 *     .value<int>()
 *     .defaultValue(1)
 *     .help("Number of parallel jobs");
 *
 * app.positional("input")
 *     .required()
 *     .help("Input file");
 *
 * auto result = app.parse(argc, argv);
 * if (!result) {
 *     std::cerr << result.error().message << '\n';
 *     return 2;
 * }
 * if (result->helpRequested()) {
 *     std::cout << app.help();
 *     return 0;
 * }
 * @endcode
 *
 * Design notes (see the accompanying design discussion for full rationale):
 *  - No std::expected: this targets C++20, so Result<T> is a small hand-rolled
 *    equivalent with the same operator bool()/operator*()/operator->()/error() shape.
 *  - Declarations + templated glue live here; the actual tokenizing, matching, and
 *    validation engine lives in CLI.cpp and is compiled once into a static library
 *    that every tool links against.
 *  - Naming is deliberately snake_case for the fluent builder surface (value<T>(),
 *    defaultValue(), conflictsWith(), helpRequested(), ...), mirroring the
 *    Rust `clap`-style API this was modeled after. That's a deliberate departure
 *    from BlackthornEngine's usual camelCase convention, scoped to this tool-only
 *    library -- flag it if that's not what you want.
 */

#include <any>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Blackthorn::Tools {

/**
 * @brief Describes why a parse (or parser setup) failed.
 */
struct ParseError {
	/// @brief Broad category of failure, useful for programmatic handling.
	enum class Code {
		UnknownOption,       ///< An option token didn't match anything registered.
		MissingValue,        ///< An option that takes a value wasn't given one.
		InvalidValue,        ///< A value failed to parse, or failed choices()/range().
		MissingRequired,     ///< A required option/positional/subcommand was absent.
		ConflictingOptions,  ///< Two mutually-exclusive options were both specified.
		UnexpectedPositional,///< A positional/subcommand appeared where none was expected.
		TooManyPositionals,  ///< More positional values were given than declared.
	};

	Code code{};
	std::string message;
};

/**
 * @brief Minimal std::expected-alike (no std::expected in C++20).
 *
 * Mirrors the subset of std::expected's interface this library needs:
 * operator bool(), operator*()/operator->(), and error().
 *
 * @tparam T Type of the success value. Must be move-constructible.
 */
template <class T>
class Result {
public:
	Result(T value) : value_(std::move(value)) {}
	Result(ParseError error) : error_(std::move(error)) {}

	/// @brief True if this holds a value rather than an error.
	explicit operator bool() const noexcept { return value_.has_value(); }

	T& operator*() { return *value_; }
	const T& operator*() const { return *value_; }
	T* operator->() { return &*value_; }
	const T* operator->() const { return &*value_; }

	/// @brief Precondition: !*this. Behavior is undefined if this holds a value.
	const ParseError& error() const { return error_; }

private:
	std::optional<T> value_;
	ParseError error_;
};

/**
 * @brief Trait specialized per-type to convert a raw command-line token to T.
 *
 * Built-in specializations are provided for bool, the common integer/floating
 * types, and std::string (see CLI.cpp). Specialize this for your own types to
 * use `.value<T>()` and `.choices({"a", "b"})` (the string-list overload).
 *
 * If you'd rather not write a specialization, use the
 * `.choices({{"a", MyEnum::A}, {"b", MyEnum::B}})` overload instead, which is
 * fully self-contained and needs no Parser<T> at all.
 *
 * @code
 * template <>
 * struct Blackthorn::Tools::Parser<Format> {
 *     static Blackthorn::Tools::Result<Format> parse(std::string_view text) {
 *         if (text == "text") return Format::text;
 *         if (text == "json") return Format::json;
 *         return Blackthorn::Tools::ParseError{
 *             Blackthorn::Tools::ParseError::Code::InvalidValue,
 *             "unrecognized format"
 *         };
 *     }
 * };
 * @endcode
 */
template <class T>
struct Parser {
	// Intentionally empty: specialize this for T, or use the choices() overload
	// that takes an explicit std::pair<std::string, T> mapping instead.
};

// Built-in specializations. Declared here so every translation unit can see
// they exist (required for HasParser<T> detection and overload resolution);
// defined once in CLI.cpp.
template <> struct Parser<bool> { static Result<bool> parse(std::string_view text); };
template <> struct Parser<int> { static Result<int> parse(std::string_view text); };
template <> struct Parser<long> { static Result<long> parse(std::string_view text); };
template <> struct Parser<long long> { static Result<long long> parse(std::string_view text); };
template <> struct Parser<unsigned> { static Result<unsigned> parse(std::string_view text); };
template <> struct Parser<unsigned long> { static Result<unsigned long> parse(std::string_view text); };
template <> struct Parser<unsigned long long> { static Result<unsigned long long> parse(std::string_view text); };
template <> struct Parser<float> { static Result<float> parse(std::string_view text); };
template <> struct Parser<double> { static Result<double> parse(std::string_view text); };
template <> struct Parser<std::string> { static Result<std::string> parse(std::string_view text); };

namespace Detail {

/// @brief SFINAE detector: true if Parser<T>::parse(std::string_view) exists.
template <class T, class = void>
struct HasParser : std::false_type {};

template <class T>
struct HasParser<T, std::void_t<decltype(Parser<T>::parse(std::declval<std::string_view>()))>>
	: std::true_type {};

/// @brief Human-readable type name used to build help-text meta variables.
/// Falls back to "value" for types without a more specific overload below
/// (see CLI.cpp for the built-in ones); this generic body needs no
/// specialization mechanics since it's a plain function template.
template <class T>
std::string typeName() {
	return "value";
}

template <> std::string typeName<bool>();
template <> std::string typeName<int>();
template <> std::string typeName<long>();
template <> std::string typeName<long long>();
template <> std::string typeName<unsigned>();
template <> std::string typeName<unsigned long>();
template <> std::string typeName<unsigned long long>();
template <> std::string typeName<float>();
template <> std::string typeName<double>();
template <> std::string typeName<std::string>();

} // namespace Detail

/**
 * @brief Schema for a single registered option (flag or value).
 *
 * Constructed and mutated via OptionBuilder/TypedOptionBuilder<T>; not intended
 * to be built directly by callers.
 */
struct OptionSpec {
	std::string longName;
	std::optional<char> shortName;

	std::string description;
	std::string metaVar;

	bool takesValue = false;
	bool required = false;
	bool repeatable = false;

	std::any defaultValue;

	std::vector<std::string> conflicts;
	std::vector<std::string> requirements;

	/// @brief Allowed values for help text. An empty `.second` marks "string-only"
	/// mode (validated then handed to Parser<T>); a populated `.second` marks
	/// "self-contained pair" mode (the any IS the converted value, no Parser<T> used).
	std::vector<std::pair<std::string, std::any>> choices;

	/// @brief Converts one raw token to std::any(T). Unset for flags.
	std::function<Result<std::any>(std::string_view)> parseValue;

	/// @brief Appends a converted value into a std::any holding std::vector<T>,
	/// default-constructing that vector on first use. Set alongside parseValue
	/// whenever the option takes a value; only actually invoked if repeatable.
	std::function<void(std::any&, std::any)> appendValue;

	/// @brief Optional post-parse validator (e.g. range()). Returns an error
	/// message if the value is invalid, std::nullopt otherwise.
	std::function<std::optional<std::string>(const std::any&)> rangeValidator;
};

/**
 * @brief Schema for a single registered positional argument.
 */
struct PositionalSpec {
	std::string name;
	std::string description;

	bool required = false;
	bool repeatable = false; ///< Only the last-declared positional may set this.
};

class Command;
class ParseResult;

template <class T>
class TypedOptionBuilder;

/**
 * @brief Fluent builder returned by Command::option() before `.value<T>()`/`.flag()`
 * commits it to a type. Chain immediately; see the note on Command::option().
 */
class OptionBuilder {
public:
	OptionBuilder& help(std::string text);
	OptionBuilder& required();
	OptionBuilder& repeatable();
	OptionBuilder& conflictsWith(std::string_view otherOptionName);
	OptionBuilder& requires_option(std::string_view otherOptionName);

	/// @brief Marks this option as a boolean presence flag (no value).
	OptionBuilder& flag();

	/// @brief Marks this option as taking a value of type T, returning a
	/// typed builder for `.defaultValue()`, `.choices()`, and `.range()`.
	template <class T>
	TypedOptionBuilder<T> value();

private:
	friend class Command;
	explicit OptionBuilder(OptionSpec* spec) : spec_(spec) {}

	OptionSpec* spec_;
};

/**
 * @brief Fluent builder for a value-typed option, returned by OptionBuilder::value<T>().
 */
template <class T>
class TypedOptionBuilder {
public:
	TypedOptionBuilder& help(std::string text) {
		spec_->description = std::move(text);
		return *this;
	}

	TypedOptionBuilder& required() {
		spec_->required = true;
		return *this;
	}

	TypedOptionBuilder& repeatable() {
		spec_->repeatable = true;
		return *this;
	}

	TypedOptionBuilder& conflictsWith(std::string_view otherOptionName) {
		spec_->conflicts.emplace_back(otherOptionName);
		return *this;
	}

	TypedOptionBuilder& requires_option(std::string_view otherOptionName) {
		spec_->requirements.emplace_back(otherOptionName);
		return *this;
	}

	/// @brief Sets the value used when this option is not specified.
	TypedOptionBuilder& defaultValue(T value) {
		spec_->defaultValue = std::any(std::move(value));
		return *this;
	}

	/**
	 * @brief Restricts input to a fixed set of raw strings, converted via Parser<T>.
	 * Requires a Parser<T> specialization to exist; use the pair overload below
	 * if you'd rather not write one.
	 */
	TypedOptionBuilder& choices(std::vector<std::string> allowed);

	/**
	 * @brief Restricts input to a fixed set of (string, T) pairs. Self-contained:
	 * no Parser<T> specialization is required, since the mapping IS the parser.
	 */
	TypedOptionBuilder& choices(std::vector<std::pair<std::string, T>> mapping);

	/// @brief Rejects values outside [min, max]. T must be arithmetic.
	TypedOptionBuilder& range(T min, T max);

private:
	friend class OptionBuilder;
	explicit TypedOptionBuilder(OptionSpec* spec) : spec_(spec) {}

	OptionSpec* spec_;
};

/**
 * @brief Fluent builder returned by Command::positional().
 */
class PositionalBuilder {
public:
	PositionalBuilder& help(std::string text);
	PositionalBuilder& required();

	/// @brief Marks this as a variadic positional collecting all remaining
	/// positional tokens. Only the last-declared positional may call this.
	PositionalBuilder& repeatable();

private:
	friend class Command;
	explicit PositionalBuilder(PositionalSpec* spec) : spec_(spec) {}

	PositionalSpec* spec_;
};

/**
 * @brief The outcome of a successful Command::parse() at one command level.
 *
 * If the command had subcommands and one was invoked, subcommand() returns a
 * pointer to that subcommand's own ParseResult (recursively, a subcommand
 * may itself have subcommands).
 */
class ParseResult {
public:
	/**
	 * @brief Retrieves a scalar option's value.
	 * @tparam T Must match the type used at `.value<T>()` registration
	 * (or `bool` for flags).
	 * @pre The option was either specified by the user or has a defaultValue();
	 * otherwise this throws (see raw_value()).
	 */
	template <class T>
	const T& get(std::string_view name) const;

	/**
	 * @brief Retrieves a repeatable option's collected values, in the order given.
	 * @pre The option was declared `.repeatable()` and specified at least once.
	 */
	template <class T>
	const std::vector<T>& get_all(std::string_view name) const;

	/// @brief True if the user explicitly specified this option (independent
	/// of whether a defaultValue() exists).
	bool specified(std::string_view name) const;

	/// @brief All positional tokens consumed at this command level, in order.
	std::span<const std::string> positional() const;

	/// @brief A single named (non-repeatable) positional's value.
	const std::string& positional(std::string_view name) const;

	/// @brief A repeatable named positional's collected values.
	const std::vector<std::string>& positional_all(std::string_view name) const;

	/// @brief True if -h/--help was seen at this command level.
	bool helpRequested() const;

	/// @brief If -h/--help was immediately followed by a recognized option,
	/// positional, or subcommand name (e.g. `-h --dry-run`, `--help output`,
	/// or `--help pack`), that name so the caller can show focused help via
	/// Command::help(name) instead of the full listing. std::nullopt for a
	/// bare -h/--help with nothing recognized after it.
	std::optional<std::string_view> helpTopic() const;

	/// @brief The name of the Command this result belongs to.
	std::string_view command_name() const;

	/// @brief The invoked child command's result, or nullptr if this command
	/// has no subcommands or none was selected.
	const ParseResult* subcommand() const;

private:
	friend class Command;

	const std::any& raw_value(std::string_view name) const;

	std::string commandName_;
	std::unordered_map<std::string, std::any> values_;
	std::unordered_map<std::string, std::string> positionalsByName_;
	std::unordered_map<std::string, std::vector<std::string>> positionalListsByName_;
	std::vector<std::string> positionalTokens_;
	std::set<std::string> specifiedOptions_;
	bool helpRequested_ = false;
	std::string helpTopic_;
	std::unique_ptr<ParseResult> subResult_;
};

/**
 * @brief A single command, and (via command()) the root of a tree of subcommands.
 *
 * Not copyable. Construct one per tool/subcommand and hold it by value or
 * behind a stable reference for the program's lifetime.
 *
 * @note option()/positional()/command() return builders/references that stay
 * valid for the Command's lifetime, but a fluent chain off one of them
 * (`app.option(...).flag().help(...)`) must complete as a single expression
 * before the next `option()`/`positional()`/`command()` call. Don't store
 * the intermediate builder in a variable and call `option()` again in between.
 */
class Command {
public:
	explicit Command(std::string name, std::string description = {});

	Command(const Command&) = delete;
	Command& operator=(const Command&) = delete;
	Command(Command&&) = default;
	Command& operator=(Command&&) = default;

	/// @brief Registers an option. `shortName` is optional (e.g. 'v' for -v).
	OptionBuilder option(std::string longName, std::optional<char> shortName = std::nullopt);

	/// @brief Registers a positional argument, in declaration order.
	PositionalBuilder positional(std::string name);

	/**
	 * @brief Registers a child command (e.g. `tool pack ...`) and returns a
	 * reference to it, valid for this Command's lifetime, to configure further.
	 *
	 * A Command with subcommands registered treats the first positional-shaped
	 * token as the subcommand selector; a subcommand is required unless -h/--help
	 * is given. Declaring both subcommands AND positionals on the same Command
	 * is not supported because the positionals would be unreachable.
	 */
	Command& command(std::string name, std::string description = {});

	/// @brief Parses argv (argv[0], the program path, is skipped automatically).
	Result<ParseResult> parse(int argc, char** argv) const;

	/// @brief Full help text for this command: usage, options, positionals,
	/// and (if any) subcommands.
	std::string help() const;

	/// @brief Just the "Usage: name [OPTIONS] ..." line, no trailing sections.
	/// Useful for short error messages that shouldn't dump the full listing.
	std::string usage() const;

	/// @brief Detailed help text for exactly one option or positional by name.
	/// Returns a "no such option" message rather than throwing if not found.
	std::string help(std::string_view name) const;

private:
	Result<ParseResult> parseArgs(std::span<const std::string_view> args) const;

	OptionSpec* findOption(std::string_view name);
	const OptionSpec* findOption(std::string_view name) const;
	const OptionSpec* findShortOption(char shortName) const;
	Command* findSubcommand(std::string_view name) const;

	std::string name_;
	std::string description_;

	std::deque<OptionSpec> options_;
	std::deque<PositionalSpec> positionals_;
	std::vector<std::unique_ptr<Command>> subcommands_;
};

template <class T>
const T& ParseResult::get(std::string_view name) const {
	return std::any_cast<const T&>(raw_value(name));
}

template <class T>
const std::vector<T>& ParseResult::get_all(std::string_view name) const {
	return std::any_cast<const std::vector<T>&>(raw_value(name));
}

template <class T>
TypedOptionBuilder<T> OptionBuilder::value() {
	spec_->takesValue = true;
	spec_->metaVar = Detail::typeName<T>();

	if constexpr (Detail::HasParser<T>::value) {
		spec_->parseValue = [](std::string_view raw) -> Result<std::any> {
			auto parsed = Parser<T>::parse(raw);
			if (!parsed)
				return parsed.error();

			return std::any(std::move(*parsed));
		};
	} else {
		spec_->parseValue = [](std::string_view) -> Result<std::any> {
			return ParseError{
				ParseError::Code::InvalidValue,
				"this option has no default parser for its type; register one via "
				"a Parser<T> specialization, or use choices() with an explicit "
				"std::pair<std::string, T> mapping"
			};
		};
	}

	spec_->appendValue = [](std::any& storage, std::any value) {
		if (!storage.has_value())
			storage = std::vector<T>{};

		std::any_cast<std::vector<T>&>(storage).push_back(std::any_cast<T&&>(std::move(value)));
	};

	return TypedOptionBuilder<T>(spec_);
}

template <class T>
TypedOptionBuilder<T>& TypedOptionBuilder<T>::choices(std::vector<std::string> allowed) {
	spec_->choices.clear();
	for (const auto& value : allowed)
		spec_->choices.emplace_back(value, std::any{});

	spec_->parseValue = [allowed = std::move(allowed)](std::string_view raw) -> Result<std::any> {
		const std::string rawStr(raw);
		bool matched = false;
		for (const auto& candidate : allowed) {
			if (candidate == rawStr) {
				matched = true;
				break;
			}
		}

		if (!matched) {
			std::string list;
			for (size_t i = 0; i < allowed.size(); ++i) {
				if (i != 0)
					list += ", ";
				list += allowed[i];
			}

			return ParseError{
				ParseError::Code::InvalidValue,
				"invalid value '" + rawStr + "'; expected one of: " + list
			};
		}

		auto parsed = Parser<T>::parse(raw);
		if (!parsed)
			return parsed.error();
		return std::any(std::move(*parsed));
	};

	return *this;
}

template <class T>
TypedOptionBuilder<T>& TypedOptionBuilder<T>::choices(std::vector<std::pair<std::string, T>> mapping) {
	spec_->choices.clear();
	for (const auto& [key, value] : mapping)
		spec_->choices.emplace_back(key, std::any(value));

	spec_->parseValue = [mapping = std::move(mapping)](std::string_view raw) -> Result<std::any> {
		const std::string rawStr(raw);
		for (const auto& [key, value] : mapping) {
			if (key == rawStr)
				return std::any(value);
		}

		std::string list;
		for (size_t i = 0; i < mapping.size(); ++i) {
			if (i != 0)
				list += ", ";

			list += mapping[i].first;
		}
		return ParseError{
			ParseError::Code::InvalidValue,
			"invalid value '" + rawStr + "'; expected one of: " + list
		};
	};

	return *this;
}

template <class T>
TypedOptionBuilder<T>& TypedOptionBuilder<T>::range(T min, T max) {
	static_assert(std::is_arithmetic_v<T>, "range() requires an arithmetic option type");

	spec_->rangeValidator = [min, max](const std::any& value) -> std::optional<std::string> {
		const T& v = std::any_cast<const T&>(value);
		if (v < min || v > max) {
			return "must be between " + std::to_string(min) + " and " + std::to_string(max);
		}

		return std::nullopt;
	};

	return *this;
}

} // namespace Blackthorn::Tools