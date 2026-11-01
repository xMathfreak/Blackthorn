#pragma once

#include <deque>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

#include "Core/Export.h"
#include "Types/Numeric.h"

namespace Blackthorn::Assets {

class NameTable {
public:
	NameTable() = default;

	NameTable(const NameTable&) = delete;
	NameTable& operator=(const NameTable&) = delete;

	U32 intern(std::string_view name) {
		if (const auto it = ids.find(name); it != ids.end())
			return it->second;

		if (names.size() >= U32_MAX)
			throw std::length_error("NameTable: Name table exhausted");

		const U32 id = static_cast<U32>(names.size());

		names.emplace_back(name);

		try {
			ids.emplace(std::string_view(names.back()), id);
		} catch (...) {
			names.pop_back();
			throw;
		}

		return id;
	}

	std::optional<U32> find(std::string_view name) const {
		auto it = ids.find(name);
		if (it != ids.end())
			return it->second;

		return std::nullopt;
	}

	std::string_view nameOf(U32 id) const { return names.at(id); }
	size_t size() const noexcept { return names.size(); }

private:
	std::deque<std::string> names;
	std::unordered_map<std::string_view, U32> ids;
};

} // namespace Blackthorn::Assets