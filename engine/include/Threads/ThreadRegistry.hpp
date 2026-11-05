#pragma once

#include <sstream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace Blackthorn::Threads {

/**
 * @brief Maps `std::thread::id` values to human readable names for logging.
 */
class ThreadRegistry {
public:
	static ThreadRegistry& instance() {
		static ThreadRegistry reg;
		return reg;
	}

	void registerCurrent(const std::string& name) {
		std::lock_guard<std::mutex> lock(mutex);
		names[std::this_thread::get_id()] = name;
	}

	std::string currentName() const {
		auto id = std::this_thread::get_id();

		{
			std::lock_guard<std::mutex> lock(mutex);
			auto it = names.find(id);
			if (it != names.end())
				return it->second;
		}

		// Unregistered thread
		std::ostringstream ss;
		ss << "Thread-" << id;
		return ss.str();
	}

	void unregisterCurrent() {
		std::lock_guard<std::mutex> lock(mutex);
		names.erase(std::this_thread::get_id());
	}

private:
	ThreadRegistry() = default;

	mutable std::mutex mutex;
	std::unordered_map<std::thread::id, std::string> names;
};

} // namespace Blackthorn::Debug