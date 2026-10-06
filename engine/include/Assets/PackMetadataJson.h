#pragma once

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "Assets/PackFormat.h"
#include "Types/SemVer.h"

/**
 * @file PackMetadataJson.h
 * @brief Header-only JSON <-> PackMetadata conversion, shared by btpacker
 * (source file -> embedded block) and PackMount (embedded block -> runtime).
 *
 * Keeping one implementation guarantees the packer, the runtime and any
 * future tool agree on what a valid metadata block is, and that the embedded
 * bytes are canonical (see serializePackMetadata()).
 */

namespace Blackthorn::Assets {

/// Receives one human-readable warning per dropped or suspicious field.
using PackMetadataWarnFn = std::function<void(const std::string&)>;

/**
 * @brief Returns true if @p id is a valid pack ID.
 *
 * Valid IDs are 1-128 characters from [A-Za-z0-9._-], compared
 * case-sensitively byte for byte, and neither start nor end with '.'.
 * Reverse-domain style ("com.example.mod") is the convention.
 */
inline bool isValidPackID(std::string_view id) noexcept {
	if (id.empty() || id.size() > 128)
		return false;

	if (id.front() == '.' || id.back() == '.')
		return false;

	for (const char c : id) {
		const bool ok = (c >= 'a' && c <= 'z')
			|| (c >= 'A' && c <= 'Z')
			|| (c >= '0' && c <= '9')
			|| c == '.' || c == '_' || c == '-';

		if (!ok)
			return false;
	}

	return true;
}

namespace Detail {

inline void sortUnique(std::vector<std::string>& v) {
	std::sort(v.begin(), v.end());
	v.erase(std::unique(v.begin(), v.end()), v.end());
}

/**
 * @brief Reads an optional string field. A present-but-non-string value is
 * dropped with a warning; an absent field is silently empty.
 */
inline std::string readMetadataString(
	const nlohmann::json& json,
	const char* key,
	const PackMetadataWarnFn& warn
) {
	const auto it = json.find(key);
	if (it == json.end())
		return {};

	if (!it->is_string()) {
		if (warn)
			warn(std::string("field '") + key + "' is not a string, ignoring");

		return {};
	}

	return it->get<std::string>();
}

/**
 * @brief Reads a list of pack IDs. Entries may be plain strings or objects
 * with an "id" string. Invalid, self-referencing or non-string entries are
 * dropped with a warning; the result is sorted and de-duplicated.
 */
inline std::vector<std::string> readMetadataIDList(
	const nlohmann::json& json,
	const char* key,
	const std::string& selfID,
	const PackMetadataWarnFn& warn
) {
	std::vector<std::string> out;

	const auto it = json.find(key);
	if (it == json.end())
		return out;

	if (!it->is_array()) {
		if (warn)
			warn(std::string("field '") + key + "' is not an array, ignoring");

		return out;
	}

	for (const nlohmann::json& entry : *it) {
		std::string id;

		if (entry.is_string()) {
			id = entry.get<std::string>();
		} else if (entry.is_object() && entry.contains("id") && entry["id"].is_string()) {
			id = entry["id"].get<std::string>();
		} else {
			if (warn)
				warn(std::string("'") + key + "' entry is not an ID string, ignoring");

			continue;
		}

		if (!isValidPackID(id)) {
			if (warn)
				warn(std::string("'") + key + "' entry '" + id + "' is not a valid pack ID, ignoring");

			continue;
		}

		if (!selfID.empty() && id == selfID) {
			if (warn)
				warn(std::string("'") + key + "' lists the pack's own ID, ignoring");

			continue;
		}

		out.push_back(std::move(id));
	}

	sortUnique(out);
	return out;
}

inline std::vector<PackDependency> readMetadataDependencies(
	const nlohmann::json& json,
	const std::string& selfID,
	const PackMetadataWarnFn& warn
) {
	std::vector<PackDependency> out;

	const auto it = json.find("dependencies");
	if (it == json.end())
		return out;

	if (!it->is_array()) {
		if (warn)
			warn("field 'dependencies' is not an array, ignoring");

		return out;
	}

	for (const nlohmann::json& entry : *it) {
		PackDependency dep;

		if (entry.is_string()) {
			dep.id = entry.get<std::string>();
		} else if (entry.is_object() && entry.contains("id") && entry["id"].is_string()) {
			dep.id = entry["id"].get<std::string>();

			const auto ver = entry.find("version");
			if (ver != entry.end()) {
				if (!ver->is_string()) {
					if (warn)
						warn("dependency '" + dep.id + "': 'version' is not a string, ignoring dependency");

					continue;
				}

				dep.versionRange = ver->get<std::string>();
			}
		} else {
			if (warn)
				warn("'dependencies' entry is not an ID string or an object with an 'id', ignoring");

			continue;
		}

		if (!isValidPackID(dep.id)) {
			if (warn)
				warn("dependency '" + dep.id + "' is not a valid pack ID, ignoring");

			continue;
		}

		if (!selfID.empty() && dep.id == selfID) {
			if (warn)
				warn("'dependencies' lists the pack's own ID, ignoring");

			continue;
		}

		if (!SemVerRange::parse(dep.versionRange)) {
			if (warn)
				warn("dependency '" + dep.id + "': version range '" + dep.versionRange
					+ "' is not valid, ignoring dependency");

			continue;
		}

		out.push_back(std::move(dep));
	}

	std::sort(out.begin(), out.end());
	out.erase(std::unique(out.begin(), out.end()), out.end());

	return out;
}

inline bool containsID(const std::vector<std::string>& sortedIDs, const std::string& id) {
	return std::binary_search(sortedIDs.begin(), sortedIDs.end(), id);
}

} // namespace Detail

/**
 * @brief Builds a PackMetadata from a parsed JSON object.
 *
 * Never throws. Every field is independently optional; a field that is
 * present but malformed is dropped with a warning through @p warn (which may
 * be empty), leaving the rest intact. List fields are sorted and
 * de-duplicated so the result is canonical regardless of source order.
 *
 * Unknown keys are ignored, so newer packs stay readable by older readers.
 *
 * @param json Parsed JSON. If it is not an object, an empty PackMetadata is returned.
 * @param warn Optional sink for per-field warnings.
 */
inline PackMetadata parsePackMetadata(
	const nlohmann::json& json,
	const PackMetadataWarnFn& warn = {}
) {
	PackMetadata md;

	if (!json.is_object())
		return md;

	md.id = Detail::readMetadataString(json, "id", warn);
	if (!md.id.empty() && !isValidPackID(md.id)) {
		if (warn)
			warn("id '" + md.id + "' is not a valid pack ID (1-128 chars of A-Z a-z 0-9 . _ -), ignoring");

		md.id.clear();
	}

	md.name             = Detail::readMetadataString(json, "name", warn);
	md.shortDescription = Detail::readMetadataString(json, "shortDescription", warn);
	md.longDescription  = Detail::readMetadataString(json, "longDescription", warn);
	md.author           = Detail::readMetadataString(json, "author", warn);
	md.version          = Detail::readMetadataString(json, "version", warn);

	if (!md.id.empty() && !md.version.empty() && !SemVer::parse(md.version) && warn) {
		warn("version '" + md.version + "' is not valid SemVer (e.g. 1.2.3); "
			"other packs' dependency ranges cannot match it");
	}

	md.dependencies = Detail::readMetadataDependencies(json, md.id, warn);
	md.loadBefore   = Detail::readMetadataIDList(json, "loadBefore", md.id, warn);
	md.loadAfter    = Detail::readMetadataIDList(json, "loadAfter", md.id, warn);
	md.conflicts    = Detail::readMetadataIDList(json, "conflicts", md.id, warn);

	if (warn) {
		for (const std::string& id : md.loadBefore) {
			if (Detail::containsID(md.loadAfter, id))
				warn("'" + id + "' is in both 'loadBefore' and 'loadAfter'; this can never be satisfied");
		}

		for (const PackDependency& dep : md.dependencies) {
			if (Detail::containsID(md.conflicts, dep.id))
				warn("'" + dep.id + "' is both a dependency and a conflict; this can never be satisfied");
		}
	}

	return md;
}

/**
 * @brief Serializes @p md to the canonical compact JSON embedded in a pack.
 *
 * Every key is always present (empty string / empty array when unset) and
 * keys are emitted in sorted order, so the same PackMetadata always produces
 * byte-identical output. That property is what lets the pack digest cover
 * the metadata block.
 */
inline std::string serializePackMetadata(const PackMetadata& md) {
	nlohmann::json j = nlohmann::json::object();

	j["id"] = md.id;
	j["name"] = md.name;
	j["shortDescription"] = md.shortDescription;
	j["longDescription"] = md.longDescription;
	j["author"] = md.author;
	j["version"] = md.version;

	nlohmann::json deps = nlohmann::json::array();
	for (const PackDependency& dep : md.dependencies) {
		nlohmann::json d = nlohmann::json::object();
		d["id"] = dep.id;
		d["version"] = dep.versionRange;
		deps.push_back(std::move(d));
	}

	j["dependencies"] = std::move(deps);
	j["loadBefore"] = md.loadBefore;
	j["loadAfter"] = md.loadAfter;
	j["conflicts"] = md.conflicts;

	return j.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

} // namespace Blackthorn::Assets
