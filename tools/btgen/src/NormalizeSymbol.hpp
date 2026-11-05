#pragma once

#include <cctype>
#include <string>
#include <string_view>
#include <unordered_set>

namespace BTGen {

inline std::string normalizeVariableName(std::string_view input) {
	static const std::unordered_set<std::string> reserved = {
		"alignas", "alignof", "and", "asm", "auto", "bool",
		"break", "case", "catch", "char", "class", "const",
		"constexpr", "continue", "default", "delete", "do",
		"double", "else", "enum", "explicit", "export",
		"extern", "false", "float", "for", "friend", "goto",
		"if", "inline", "int", "long", "namespace", "new",
		"noexcept", "not", "nullptr", "operator", "private",
		"protected", "public", "register", "reinterpret_cast",
		"return", "short", "signed", "sizeof", "static",
		"struct", "switch", "template", "this", "thread_local",
		"throw", "true", "try", "typedef", "typeid", "typename",
		"union", "unsigned", "using", "virtual", "void",
		"volatile", "while"
	};

	std::string result;
	bool lastWasUnderscore = false;

	for (unsigned char c : input)
	{
		if (std::isalnum(c) || c == '_')
		{
			result += c;
			lastWasUnderscore = (c == '_');
		}
		else if (!lastWasUnderscore)
		{
			result += '_';
			lastWasUnderscore = true;
		}
	}

	// Remove leading and trailing underscores.
	while (!result.empty() && result.front() == '_')
		result.erase(result.begin());

	while (!result.empty() && result.back() == '_')
		result.pop_back();

	// Handle empty strings.
	if (result.empty())
		result = "variable";

	// Ensure the name doesn't start with a digit.
	if (std::isdigit(static_cast<unsigned char>(result[0])))
		result = "_" + result;

	// Avoid C++ keywords.
	if (reserved.count(result))
		result += "_";

	return result;
}

} // namespace BTGen