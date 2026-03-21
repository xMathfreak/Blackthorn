#include "Core/Settings.h"

#include <fstream>

namespace Blackthorn::Core {

Settings& Settings::instance() {
	static Settings inst;
	return inst;
}

bool Settings::loadFromFile(const std::string& path) {
	std::lock_guard<std::mutex> lock(mutex);

	groupedValues.clear();
	sectionOrder.clear();
	dirty = false;

	std::ifstream file(path);
	if (!file.is_open())
		return false;

	const auto notSpace = [](unsigned char c){ return !std::isspace(c); };

	auto trimStr = [&](std::string s) -> std::string {
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
		s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
		return s;
	};

	std::string currentSection;
	std::string line;

	while (std::getline(file, line)) {
		for (char c : {'#', ';'}) {
			auto pos = line.find(c);
			if (pos != std::string::npos)
				line = line.substr(0, pos);
		}

		line = trimStr(line);
		if (line.empty())
			continue;

		if (line.front() == '[' && line.back() == ']') {
			currentSection = normalize(line.substr(1, line.size() - 2));
			ensureSectionOrder(currentSection);
			continue;
		}

		const auto eq = line.find('=');
		if (eq == std::string::npos)
			continue;

		const std::string key   = normalize(line.substr(0, eq));
		const std::string value = trimStr(line.substr(eq + 1));

		if (!key.empty()) {
			auto& sd = groupedValues[currentSection];
			if (sd.values.find(key) == sd.values.end())
				sd.keyOrder.push_back(key);

			sd.values[key] = value;
		}
	}

	return true;
}

bool Settings::saveToFile(const std::string& path) {
	std::lock_guard<std::mutex> lock(mutex);

	std::ofstream out(path, std::ios::out | std::ios::trunc);
	if (!out.is_open())
		return false;

	for (const auto& sec : sectionOrder) {
		auto it = groupedValues.find(sec);
		if (it == groupedValues.end())
			continue;

		out << '[' << sec << "]\n";

		for (const auto& k : it->second.keyOrder) {
			auto vit = it->second.values.find(k);
			if (vit != it->second.values.end())
				out << k << " = " << vit->second << '\n';
		}

		out << '\n';
	}

	dirty = false;
	return true;
}

void Settings::onChange(const std::string& section, const std::string& key, std::function<void(const std::string&)> callback) {
	const std::string cbKey = normalize(section) + "|" + normalize(key);
	std::lock_guard<std::mutex> lock(mutex);
	changeCallbacks[cbKey].push_back(std::move(callback));
}

bool Settings::isDirty() const {
	std::lock_guard<std::mutex> lock(mutex);
	return dirty;
}

void Settings::markClean() {
	std::lock_guard<std::mutex> lock(mutex);
	dirty = false;
}

bool Settings::hasSection(const std::string& section) const {
	std::lock_guard<std::mutex> lock(mutex);
	return groupedValues.count(normalize(section)) > 0;
}

bool Settings::hasKey(const std::string& section, const std::string& key) const {
	std::lock_guard<std::mutex> lock(mutex);
	auto it = groupedValues.find(normalize(section));
	if (it == groupedValues.end())
		return false;
	return it->second.values.count(normalize(key)) > 0;
}

void Settings::remove(const std::string& section, const std::string& key) {
	std::lock_guard<std::mutex> lock(mutex);

	auto it = groupedValues.find(normalize(section));
	if (it == groupedValues.end())
		return;

	const auto k = normalize(key);
	it->second.values.erase(k);
	auto& order = it->second.keyOrder;
	order.erase(std::remove(order.begin(), order.end(), k), order.end());
	dirty = true;
}

void Settings::removeSection(const std::string& section) {
	std::lock_guard<std::mutex> lock(mutex);
	const auto sec = normalize(section);
	groupedValues.erase(sec);

	sectionOrder.erase(
		std::remove(sectionOrder.begin(), sectionOrder.end(), sec),
		sectionOrder.end()
	);

	dirty = true;
}

void Settings::clear() {
	std::lock_guard<std::mutex> lock(mutex);

	groupedValues.clear();
	sectionOrder.clear();
	changeCallbacks.clear();
	dirty = false;
}

} // namespace Blackthorn::Core