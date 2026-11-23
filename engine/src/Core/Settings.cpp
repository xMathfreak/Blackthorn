#include "Blackthorn/Core/Settings.hpp"

#include <fstream>

#include "Blackthorn/Debug/Logger.hpp"
#include "StringHelper.hpp"

namespace Blackthorn::Core {

Settings& Settings::instance() {
	static Settings inst;
	return inst;
}

bool Settings::loadFromFile(const std::filesystem::path& file) {
	std::error_code ec;

	if (!std::filesystem::is_regular_file(file, ec))
		return false;

	try {
		data = toml::parse_file(file.generic_string());
	} catch (const toml::parse_error& err) {
		BT_ERROR("Settings: Failed to parse TOML file '{}'", file.generic_string());
		return false;
	}

	dirty = false;
	return true;
}

bool Settings::saveToFile(const std::filesystem::path& file) {
	std::ofstream f(file);
	if (!f) {
		BT_ERROR("Settings: Failed to create file '{}'", file.generic_string());
		return false;
	}

	dirty = false;
	f << data << '\n';
	f.close();

	return true;
}

bool Settings::remove(std::string_view section, std::string_view key) noexcept {
	if (auto* sec = data[section].as_table()) {
		sec->erase(key);
		dirty = true;
		return true;
	}

	return false;
}

bool Settings::removeSection(std::string_view section) noexcept {
	if (data.erase(section)) {
		dirty = true;
		return true;
	}

	return false;
}

void Settings::clear() noexcept {
	data.clear();
	onChangeCallbacks.clear();
	dirty = true;
}

bool Settings::hasKey(std::string_view section, std::string_view key) const noexcept {
	return static_cast<bool>(data[section][key]);
}

bool Settings::hasSection(std::string_view section) const noexcept {
	return data[section].is_table();
}

bool Settings::isDirty() const noexcept {
	return dirty;
}

std::string Settings::makeCallbackKey(std::string_view section, std::string_view key) const noexcept {
	return std::format("{} | {}", section, key);
}

void Settings::fireCallbacks(std::string_view section, std::string_view key, const toml::node* oldValue, const toml::node& newValue) const {
	auto cbKey = makeCallbackKey(section, key);

	auto it = onChangeCallbacks.find(cbKey);
	if (it == onChangeCallbacks.end())
		return;

	auto callbacks = it->second;

	for (const auto& cb : callbacks)
		cb(oldValue, newValue);
}

} // namespace Blackthorn::Core