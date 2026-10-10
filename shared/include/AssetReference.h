#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "AssetNormalize.h"

namespace Blackthorn {

/**
 * @brief Name reserved for "the root of the file that contains the reference".
 *
 * A pack or project cannot be named "root", so "@root/" is never ambiguous.
 */
inline constexpr std::string_view kReservedRootName = "root";

/**
 * @brief Splits a '/'-separated path into its segments. Input must be normalized.
 */
inline std::vector<std::string> splitAssetSegments(std::string_view path) {
	std::vector<std::string> out;
	size_t start = 0;

	while (start <= path.size()) {
		size_t end = path.find('/', start);
		if (end == std::string_view::npos)
			end = path.size();

		out.emplace_back(path.substr(start, end - start));
		start = end + 1;
	}

	return out;
}

/**
 * @brief Checks that @p name can be used as a root name.
 *
 * A root name is a single path segment: no separators, no "@", not "." or "..",
 * not empty, and not the reserved name.
 */
inline bool isValidRootName(std::string_view name) {
	if (name.empty() || name == "." || name == ".." || name == kReservedRootName)
		return false;

	return name.find_first_of("/\\:@") == std::string_view::npos;
}

/**
 * @brief Resolves a reference written inside a data file to a canonical asset ID.
 *
 * Reference forms:
 *   smoke.png              relative to the containing file's directory
 *   ../textures/x.png      relative, climbing up but never past the root
 *   @root/particles/x.png  absolute within the containing file's own root
 *   @engine/particles/x.png absolute in the named root "engine"
 *
 * Rejected: absolute paths ("/x", "C:/x"), backslashes, empty references, and
 * "@name" references without a following '/'. Climbing above a root is rejected.
 *
 * Canonical IDs always have at least two segments: root/path/to/file.
 *
 * @param baseID The canonical ID of the file that contains the reference.
 * @param ref    The reference string as written in that file.
 * @return The canonical ID, or std::nullopt if the reference or base is invalid.
 */
inline std::optional<std::string> resolveAssetReference(std::string_view baseID, std::string_view ref) {
	const auto base = normalizeAssetPath(baseID);
	if (!base)
		return std::nullopt;

	const std::vector<std::string> baseSegments = splitAssetSegments(*base);
	if (baseSegments.size() < 2)
		return std::nullopt;

	if (ref.empty() || ref.find('\\') != std::string_view::npos || ref.front() == '/')
		return std::nullopt;

	std::string rootName = baseSegments.front();
	std::vector<std::string> parts;
	std::string_view rest = ref;

	if (ref.front() == '@') {
		const size_t slash = ref.find('/');
		if (slash == std::string_view::npos)
			return std::nullopt;

		const std::string_view prefix = ref.substr(1, slash - 1);
		if (!isValidRootName(prefix) && prefix != kReservedRootName)
			return std::nullopt;

		if (prefix != kReservedRootName)
			rootName = std::string(prefix);

		rest = ref.substr(slash + 1);
	} else {
		// Start from the containing file's directory, relative to the root.
		for (size_t i = 1; i + 1 < baseSegments.size(); ++i)
			parts.push_back(baseSegments[i]);
	}

	if (rest.empty())
		return std::nullopt;

	size_t start = 0;
	while (start <= rest.size()) {
		size_t end = rest.find('/', start);
		if (end == std::string_view::npos)
			end = rest.size();

		const std::string_view segment = rest.substr(start, end - start);

		if (segment == "..") {
			if (parts.empty())
				return std::nullopt;

			parts.pop_back();
		} else if (!segment.empty() && segment != ".") {
			parts.emplace_back(segment);
		}

		start = end + 1;
	}

	if (parts.empty())
		return std::nullopt;

	std::string joined = rootName;
	for (const std::string& part : parts) {
		joined += '/';
		joined += part;
	}

	return normalizeAssetPath(joined);
}

} // namespace Blackthorn
