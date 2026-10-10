#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace Blackthorn {

/**
 * @brief Converts an asset path to its canonical form, which is also its asset ID.
 *
 * Rules:
 * - Backslashes become forward slashes.
 * - Repeated slashes collapse and a leading "./" is removed.
 * - Absolute paths, drive letters, and "." / ".." / empty segments are rejected.
 * - Comparison is case-sensitive; the case as written is kept.
 *
 * @param input Path as written by the user or the manifest.
 * @return The canonical path, or std::nullopt if the path is invalid.
 */
inline std::optional<std::string> normalizeAssetPath(std::string_view input) {
	if (input.empty() || input.front() == '/' || input.front() == '\\')
		return std::nullopt;

	if (input.size() >= 2 && input[1] == ':')
		return std::nullopt;

	std::string out;
	out.reserve(input.size());

	for (char c : input) {
		if (c == '\\')
			c = '/';

		if (c == '/' && !out.empty() && out.back() == '/')
			continue;

		out.push_back(c);
	}

	while (out.size() >= 2 && out[0] == '.' && out[1] == '/')
		out.erase(0, 2);

	if (out.empty())
		return std::nullopt;

	size_t start = 0;
	while (start <= out.size()) {
		size_t end = out.find('/', start);
		if (end == std::string::npos)
			end = out.size();

		const std::string_view segment(out.data() + start, end - start);
		if (segment.empty() || segment == "." || segment == "..")
			return std::nullopt;

		start = end + 1;
	}

	return out;
}

} // namespace Blackthorn