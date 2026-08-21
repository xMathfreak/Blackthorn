/**
 * @file CLI.cpp
 * @brief Implementation of the tokenizing/matching/validation engine declared
 * in CLI.h, plus the built-in Parser<T> and Detail::typeName<T> specializations.
 *
 * Everything here is non-template on purpose: this is compiled once into a
 * static library (see tools/CMakeLists.txt) and linked into every tool, so
 * only the genuinely type-dependent glue stays header-only.
 */

#include "CLI.h"

#include <charconv>
#include <stdexcept>

namespace Blackthorn::Tools {

// ---------------------------------------------------------------------------
// Detail::typeName<T> specializations
// ---------------------------------------------------------------------------

namespace Detail {

template <> std::string typeName<bool>() { return "bool"; }
template <> std::string typeName<int>() { return "int"; }
template <> std::string typeName<long>() { return "long"; }
template <> std::string typeName<long long>() { return "long long"; }
template <> std::string typeName<unsigned>() { return "unsigned"; }
template <> std::string typeName<unsigned long>() { return "unsigned long"; }
template <> std::string typeName<unsigned long long>() { return "unsigned long long"; }
template <> std::string typeName<float>() { return "float"; }
template <> std::string typeName<double>() { return "double"; }
template <> std::string typeName<std::string>() { return "string"; }

} // namespace Detail

// ---------------------------------------------------------------------------
// Built-in Parser<T> specializations
// ---------------------------------------------------------------------------

namespace {

/// @brief Shared implementation for every Parser<Integral>::parse() below.
template <class T>
Result<T> parseIntegral(std::string_view text) {
	T value{};
	const auto* begin = text.data();
	const auto* end = text.data() + text.size();
	auto [ptr, ec] = std::from_chars(begin, end, value);
	if (ec != std::errc{} || ptr != end) {
		return ParseError{
			ParseError::Code::InvalidValue,
			"expected an integer, got '" + std::string(text) + "'"
		};
	}
	return value;
}

/// @brief Shared implementation for every Parser<FloatingPoint>::parse() below.
template <class T>
Result<T> parseFloatingPoint(std::string_view text) {
	T value{};
	const auto* begin = text.data();
	const auto* end = text.data() + text.size();
	auto [ptr, ec] = std::from_chars(begin, end, value);
	if (ec != std::errc{} || ptr != end) {
		return ParseError{
			ParseError::Code::InvalidValue,
			"expected a number, got '" + std::string(text) + "'"
		};
	}
	return value;
}

} // namespace

Result<bool> Parser<bool>::parse(std::string_view text) {
	if (text == "true" || text == "1" || text == "yes" || text == "on")
		return true;
	if (text == "false" || text == "0" || text == "no" || text == "off")
		return false;
	return ParseError{
		ParseError::Code::InvalidValue,
		"expected a boolean (true/false), got '" + std::string(text) + "'"
	};
}

Result<int> Parser<int>::parse(std::string_view text) { return parseIntegral<int>(text); }
Result<long> Parser<long>::parse(std::string_view text) { return parseIntegral<long>(text); }
Result<long long> Parser<long long>::parse(std::string_view text) { return parseIntegral<long long>(text); }
Result<unsigned> Parser<unsigned>::parse(std::string_view text) { return parseIntegral<unsigned>(text); }
Result<unsigned long> Parser<unsigned long>::parse(std::string_view text) { return parseIntegral<unsigned long>(text); }
Result<unsigned long long> Parser<unsigned long long>::parse(std::string_view text) { return parseIntegral<unsigned long long>(text); }
Result<float> Parser<float>::parse(std::string_view text) { return parseFloatingPoint<float>(text); }
Result<double> Parser<double>::parse(std::string_view text) { return parseFloatingPoint<double>(text); }

Result<std::string> Parser<std::string>::parse(std::string_view text) {
	return std::string(text);
}

// ---------------------------------------------------------------------------
// OptionBuilder (non-template members)
// ---------------------------------------------------------------------------

OptionBuilder& OptionBuilder::help(std::string text) {
	spec_->description = std::move(text);
	return *this;
}

OptionBuilder& OptionBuilder::required() {
	spec_->required = true;
	return *this;
}

OptionBuilder& OptionBuilder::repeatable() {
	spec_->repeatable = true;
	return *this;
}

OptionBuilder& OptionBuilder::conflictsWith(std::string_view otherOptionName) {
	spec_->conflicts.emplace_back(otherOptionName);
	return *this;
}

OptionBuilder& OptionBuilder::requires_option(std::string_view otherOptionName) {
	spec_->requirements.emplace_back(otherOptionName);
	return *this;
}

OptionBuilder& OptionBuilder::flag() {
	spec_->takesValue = false;
	spec_->parseValue = nullptr;
	spec_->appendValue = nullptr;
	return *this;
}

// ---------------------------------------------------------------------------
// PositionalBuilder
// ---------------------------------------------------------------------------

PositionalBuilder& PositionalBuilder::help(std::string text) {
	spec_->description = std::move(text);
	return *this;
}

PositionalBuilder& PositionalBuilder::required() {
	spec_->required = true;
	return *this;
}

PositionalBuilder& PositionalBuilder::repeatable() {
	spec_->repeatable = true;
	return *this;
}

// ---------------------------------------------------------------------------
// ParseResult
// ---------------------------------------------------------------------------

bool ParseResult::specified(std::string_view name) const {
	return specifiedOptions_.count(std::string(name)) != 0;
}

std::span<const std::string> ParseResult::positional() const {
	return positionalTokens_;
}

const std::string& ParseResult::positional(std::string_view name) const {
	auto it = positionalsByName_.find(std::string(name));
	if (it == positionalsByName_.end()) {
		throw std::out_of_range(
			"Blackthorn::Tools::ParseResult: no positional named '" + std::string(name) + "'"
		);
	}
	return it->second;
}

const std::vector<std::string>& ParseResult::positional_all(std::string_view name) const {
	auto it = positionalListsByName_.find(std::string(name));
	if (it == positionalListsByName_.end()) {
		throw std::out_of_range(
			"Blackthorn::Tools::ParseResult: no repeatable positional named '" + std::string(name) + "'"
		);
	}
	return it->second;
}

bool ParseResult::helpRequested() const {
	return helpRequested_;
}

std::string_view ParseResult::command_name() const {
	return commandName_;
}

const ParseResult* ParseResult::subcommand() const {
	return subResult_.get();
}

const std::any& ParseResult::raw_value(std::string_view name) const {
	auto it = values_.find(std::string(name));
	if (it == values_.end()) {
		throw std::out_of_range(
			"Blackthorn::Tools::ParseResult: no value stored for option '" + std::string(name) +
			"' (it was never specified and has no default_value())"
		);
	}
	return it->second;
}

// ---------------------------------------------------------------------------
// Command: construction & registration
// ---------------------------------------------------------------------------

Command::Command(std::string name, std::string description)
	: name_(std::move(name)), description_(std::move(description)) {}

OptionBuilder Command::option(std::string longName, std::optional<char> shortName) {
	if (findOption(longName) != nullptr) {
		throw std::logic_error(
			"Blackthorn::Tools::Command('" + name_ + "'): duplicate option '--" + longName + "'"
		);
	}
	if (shortName && findShortOption(*shortName) != nullptr) {
		throw std::logic_error(
			"Blackthorn::Tools::Command('" + name_ + "'): duplicate short option '-" +
			std::string(1, *shortName) + "'"
		);
	}

	OptionSpec spec;
	spec.longName = std::move(longName);
	spec.shortName = shortName;
	spec.metaVar = "value";

	options_.push_back(std::move(spec));
	return OptionBuilder(&options_.back());
}

PositionalBuilder Command::positional(std::string name) {
	for (const auto& existing : positionals_) {
		if (existing.name == name) {
			throw std::logic_error(
				"Blackthorn::Tools::Command('" + name_ + "'): duplicate positional '" + name + "'"
			);
		}
	}
	if (!positionals_.empty() && positionals_.back().repeatable) {
		throw std::logic_error(
			"Blackthorn::Tools::Command('" + name_ + "'): positional '" + positionals_.back().name +
			"' is repeatable and must be the last positional declared"
		);
	}

	PositionalSpec spec;
	spec.name = std::move(name);

	positionals_.push_back(std::move(spec));
	return PositionalBuilder(&positionals_.back());
}

Command& Command::command(std::string name, std::string description) {
	if (findSubcommand(name) != nullptr) {
		throw std::logic_error(
			"Blackthorn::Tools::Command('" + name_ + "'): duplicate subcommand '" + name + "'"
		);
	}

	subcommands_.push_back(std::make_unique<Command>(std::move(name), std::move(description)));
	return *subcommands_.back();
}

OptionSpec* Command::findOption(std::string_view name) {
	return const_cast<OptionSpec*>(std::as_const(*this).findOption(name));
}

const OptionSpec* Command::findOption(std::string_view name) const {
	for (const auto& spec : options_) {
		if (spec.longName == name)
			return &spec;
	}
	return nullptr;
}

const OptionSpec* Command::findShortOption(char shortName) const {
	for (const auto& spec : options_) {
		if (spec.shortName && *spec.shortName == shortName)
			return &spec;
	}
	return nullptr;
}

Command* Command::findSubcommand(std::string_view name) const {
	for (const auto& child : subcommands_) {
		if (child->name_ == name)
			return child.get();
	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// Command: parsing
// ---------------------------------------------------------------------------

Result<ParseResult> Command::parse(int argc, char** argv) const {
	std::vector<std::string_view> args;
	if (argc > 1) {
		args.reserve(static_cast<size_t>(argc - 1));
		for (int i = 1; i < argc; ++i)
			args.emplace_back(argv[i]);
	}
	return parseArgs(args);
}

Result<ParseResult> Command::parseArgs(std::span<const std::string_view> args) const {
	ParseResult result;
	result.commandName_ = name_;

	// Converts one raw token to this option's stored value, running range()
	// validation afterward, and records it (appending if repeatable).
	auto recordValue = [&result](const OptionSpec& spec, std::string_view raw) -> std::optional<ParseError> {
		Result<std::any> parsed = spec.parseValue(raw);
		if (!parsed) {
			ParseError err = parsed.error();
			err.message = "--" + spec.longName + ": " + err.message;
			return err;
		}

		if (spec.rangeValidator) {
			if (auto message = spec.rangeValidator(*parsed)) {
				return ParseError{ParseError::Code::InvalidValue, "--" + spec.longName + " " + *message};
			}
		}

		if (spec.repeatable) {
			spec.appendValue(result.values_[spec.longName], std::move(*parsed));
		} else {
			result.values_[spec.longName] = std::move(*parsed);
		}
		result.specifiedOptions_.insert(spec.longName);
		return std::nullopt;
	};

	bool endOfOptions = false;
	bool subcommandConsumed = false;
	size_t positionalIndex = 0;

	// options_ is fixed for the lifetime of this call, so this only needs computing once.
	const bool helpIsReserved = findOption("help") == nullptr && findShortOption('h') == nullptr;

	for (size_t i = 0; i < args.size(); ++i) {
		const std::string_view arg = args[i];

		if (!endOfOptions && arg == "--") {
			endOfOptions = true;
			continue;
		}

		if (!endOfOptions && helpIsReserved && (arg == "-h" || arg == "--help")) {
			result.helpRequested_ = true;
			continue;
		}

		// --name or --name=value
		if (!endOfOptions && arg.size() >= 2 && arg[0] == '-' && arg[1] == '-') {
			const std::string_view body = arg.substr(2);
			std::string_view name = body;
			std::optional<std::string_view> inlineValue;
			if (const auto eq = body.find('='); eq != std::string_view::npos) {
				name = body.substr(0, eq);
				inlineValue = body.substr(eq + 1);
			}

			const OptionSpec* spec = findOption(name);
			if (!spec) {
				return ParseError{
					ParseError::Code::UnknownOption,
					"unknown option '--" + std::string(name) + "'"
				};
			}

			if (!spec->takesValue) {
				if (inlineValue) {
					return ParseError{
						ParseError::Code::InvalidValue,
						"--" + spec->longName + " does not take a value"
					};
				}
				result.values_[spec->longName] = std::any(true);
				result.specifiedOptions_.insert(spec->longName);
				continue;
			}

			std::string_view raw;
			if (inlineValue) {
				raw = *inlineValue;
			} else {
				if (i + 1 >= args.size()) {
					return ParseError{
						ParseError::Code::MissingValue,
						"--" + spec->longName + " requires a value"
					};
				}
				raw = args[++i];
			}

			if (auto err = recordValue(*spec, raw))
				return *err;
			continue;
		}

		// -x, -xyz (clustered flags, optionally ending in a value-taking option)
		if (!endOfOptions && arg.size() >= 2 && arg[0] == '-') {
			const std::string_view cluster = arg.substr(1);

			for (size_t c = 0; c < cluster.size(); ++c) {
				const char letter = cluster[c];

				if (helpIsReserved && letter == 'h') {
					result.helpRequested_ = true;
					continue;
				}

				const OptionSpec* spec = findShortOption(letter);
				if (!spec) {
					return ParseError{
						ParseError::Code::UnknownOption,
						"unknown option '-" + std::string(1, letter) + "'"
					};
				}

				if (!spec->takesValue) {
					result.values_[spec->longName] = std::any(true);
					result.specifiedOptions_.insert(spec->longName);
					continue;
				}

				std::string_view raw;
				const std::string_view rest = cluster.substr(c + 1);
				if (!rest.empty()) {
					raw = rest;
				} else {
					if (i + 1 >= args.size()) {
						return ParseError{
							ParseError::Code::MissingValue,
							"-" + std::string(1, letter) + " requires a value"
						};
					}
					raw = args[++i];
				}

				if (auto err = recordValue(*spec, raw))
					return *err;
				break; // The rest of the cluster was consumed as this option's value.
			}
			continue;
		}

		// Everything else is positional-shaped: a subcommand name, or a positional value.
		if (!subcommandConsumed && !subcommands_.empty()) {
			Command* child = findSubcommand(arg);
			if (!child) {
				std::string names;
				for (size_t k = 0; k < subcommands_.size(); ++k) {
					if (k != 0)
						names += ", ";
					names += subcommands_[k]->name_;
				}
				return ParseError{
					ParseError::Code::UnexpectedPositional,
					"unknown subcommand '" + std::string(arg) + "'; expected one of: " + names
				};
			}

			subcommandConsumed = true;
			Result<ParseResult> subResult = child->parseArgs(args.subspan(i + 1));
			if (!subResult)
				return subResult.error();

			result.subResult_ = std::make_unique<ParseResult>(std::move(*subResult));
			break;
		}

		if (positionals_.empty()) {
			return ParseError{
				ParseError::Code::UnexpectedPositional,
				"unexpected positional argument '" + std::string(arg) + "'"
			};
		}

		if (positionalIndex >= positionals_.size()) {
			const PositionalSpec& last = positionals_.back();
			if (!last.repeatable) {
				return ParseError{
					ParseError::Code::TooManyPositionals,
					"too many positional arguments (unexpected '" + std::string(arg) + "')"
				};
			}
			result.positionalListsByName_[last.name].emplace_back(arg);
			result.positionalTokens_.emplace_back(arg);
			continue;
		}

		const PositionalSpec& spec = positionals_[positionalIndex];
		result.positionalTokens_.emplace_back(arg);
		if (spec.repeatable) {
			result.positionalListsByName_[spec.name].emplace_back(arg);
			// Deliberately don't advance positionalIndex: a repeatable positional
			// must be last, so it keeps absorbing tokens for the rest of the loop.
		} else {
			result.positionalsByName_[spec.name] = std::string(arg);
			++positionalIndex;
		}
	}

	if (result.helpRequested_)
		return result;

	if (!subcommands_.empty() && !subcommandConsumed) {
		std::string names;
		for (size_t k = 0; k < subcommands_.size(); ++k) {
			if (k != 0)
				names += ", ";
			names += subcommands_[k]->name_;
		}
		return ParseError{
			ParseError::Code::MissingRequired,
			"a subcommand is required; expected one of: " + names
		};
	}

	for (const PositionalSpec& spec : positionals_) {
		const bool has = spec.repeatable
			? result.positionalListsByName_.count(spec.name) != 0
			: result.positionalsByName_.count(spec.name) != 0;
		if (spec.required && !has) {
			return ParseError{
				ParseError::Code::MissingRequired,
				"missing required positional argument '" + spec.name + "'"
			};
		}
	}

	for (const OptionSpec& spec : options_) {
		if (result.specifiedOptions_.count(spec.longName) != 0)
			continue;

		if (!spec.takesValue) {
			result.values_[spec.longName] = std::any(false);
		} else if (spec.defaultValue.has_value()) {
			result.values_[spec.longName] = spec.defaultValue;
		} else if (spec.required) {
			return ParseError{
				ParseError::Code::MissingRequired,
				"missing required option '--" + spec.longName + "'"
			};
		}
	}

	for (const OptionSpec& spec : options_) {
		if (result.specifiedOptions_.count(spec.longName) == 0)
			continue;

		for (const std::string& other : spec.conflicts) {
			if (result.specifiedOptions_.count(other) != 0) {
				return ParseError{
					ParseError::Code::ConflictingOptions,
					"--" + spec.longName + " conflicts with --" + other
				};
			}
		}
		for (const std::string& other : spec.requirements) {
			if (result.specifiedOptions_.count(other) == 0) {
				return ParseError{
					ParseError::Code::MissingRequired,
					"--" + spec.longName + " requires --" + other
				};
			}
		}
	}

	return result;
}

// ---------------------------------------------------------------------------
// Command: help text
// ---------------------------------------------------------------------------

namespace {

constexpr size_t HelpColumnWidth = 28;

std::string joinNames(const std::vector<std::string>& names) {
	std::string out;
	for (size_t i = 0; i < names.size(); ++i) {
		if (i != 0)
			out += ", ";
		out += names[i];
	}
	return out;
}

std::string padColumn(std::string text) {
	if (text.size() < HelpColumnWidth)
		text += std::string(HelpColumnWidth - text.size(), ' ');
	else
		text += "  ";
	return text;
}

std::string formatOptionUsage(const OptionSpec& spec) {
	std::string out = "    ";
	if (spec.shortName) {
		out += '-';
		out += *spec.shortName;
		out += ", ";
	} else {
		out += "    ";
	}
	out += "--" + spec.longName;
	if (spec.takesValue)
		out += " <" + spec.metaVar + ">";
	return out;
}

} // namespace

std::string Command::help() const {
	std::string out = name_;
	if (!description_.empty())
		out += " - " + description_;
	out += "\n\nUSAGE:\n";

	if (!subcommands_.empty()) {
		out += "    " + name_ + " [OPTIONS] <SUBCOMMAND> [ARGS...]\n";
	} else {
		out += "    " + name_ + " [OPTIONS]";
		for (const auto& pos : positionals_) {
			if (pos.repeatable)
				out += " <" + pos.name + "...>";
			else if (pos.required)
				out += " <" + pos.name + ">";
			else
				out += " [" + pos.name + "]";
		}
		out += "\n";
	}

	out += "\nOPTIONS:\n";
	for (const auto& spec : options_) {
		std::string line = padColumn(formatOptionUsage(spec)) + spec.description;

		std::vector<std::string> notes;
		if (spec.required)
			notes.emplace_back("required");
		if (spec.takesValue && spec.defaultValue.has_value())
			notes.emplace_back("has a default");
		if (!spec.choices.empty()) {
			std::vector<std::string> choiceNames;
			for (const auto& choice : spec.choices)
				choiceNames.push_back(choice.first);
			notes.emplace_back("one of: " + joinNames(choiceNames));
		}
		if (!spec.conflicts.empty())
			notes.emplace_back("conflicts with: " + joinNames(spec.conflicts));
		if (!spec.requirements.empty())
			notes.emplace_back("requires: " + joinNames(spec.requirements));

		if (!notes.empty()) {
			line += " (";
			for (size_t i = 0; i < notes.size(); ++i) {
				if (i != 0)
					line += "; ";
				line += notes[i];
			}
			line += ")";
		}
		out += line + "\n";
	}
	out += padColumn("    -h, --help") + "Show this help message\n";

	if (!positionals_.empty() && subcommands_.empty()) {
		out += "\nPOSITIONALS:\n";
		for (const auto& pos : positionals_) {
			std::string line = padColumn("    <" + pos.name + ">") + pos.description;
			if (pos.required)
				line += " (required)";
			out += line + "\n";
		}
	}

	if (!subcommands_.empty()) {
		out += "\nSUBCOMMANDS:\n";
		for (const auto& child : subcommands_)
			out += padColumn("    " + child->name_) + child->description_ + "\n";
		out += "\nRun '" + name_ + " <subcommand> --help' for more information on a subcommand.\n";
	}

	return out;
}

std::string Command::help(std::string_view name) const {
	if (const OptionSpec* spec = findOption(name)) {
		std::string out = formatOptionUsage(*spec) + "\n";
		if (!spec->description.empty())
			out += "    " + spec->description + "\n";
		if (spec->required)
			out += "    Required.\n";
		if (spec->takesValue && spec->defaultValue.has_value())
			out += "    Has a default value.\n";
		if (!spec->choices.empty()) {
			std::vector<std::string> choiceNames;
			for (const auto& choice : spec->choices)
				choiceNames.push_back(choice.first);
			out += "    Allowed values: " + joinNames(choiceNames) + "\n";
		}
		if (!spec->conflicts.empty())
			out += "    Conflicts with: " + joinNames(spec->conflicts) + "\n";
		if (!spec->requirements.empty())
			out += "    Requires: " + joinNames(spec->requirements) + "\n";
		return out;
	}

	for (const auto& pos : positionals_) {
		if (pos.name == name) {
			std::string out = "<" + pos.name + ">\n";
			if (!pos.description.empty())
				out += "    " + pos.description + "\n";
			if (pos.required)
				out += "    Required.\n";
			if (pos.repeatable)
				out += "    Repeatable (collects all remaining positional arguments).\n";
			return out;
		}
	}

	return "no such option or positional: '" + std::string(name) + "'";
}

} // namespace Blackthorn::Tools
