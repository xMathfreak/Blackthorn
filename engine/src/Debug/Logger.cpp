#include "Debug/Logger.h"

#include <chrono>
#include <ctime>
#include <filesystem>

#include <SDL3/SDL.h>

#include "Threads/ThreadRegistry.h"

namespace Blackthorn::Debug {

Logger& Logger::instance() {
	static Logger logger;
	return logger;
}

Logger::Logger() = default;

Logger::~Logger() {
	shutdown();
}

void Logger::init(const LoggerConfig& cfg) {
	std::lock_guard<std::mutex> lock(mutex);

	if (file.is_open()) {
		writeSessionFooter();
		file.close();
	}

	config = cfg;
	sdlMirror = cfg.mirrorToSDL;

	#ifdef BLACKTHORN_DEBUG
		currentLevel = LogLevel::Debug;
	#else
		currentLevel = LogLevel::Info;
	#endif

	openFile();

	if (file.is_open()) {
		writeSessionHeader();
		initialized = true;
	}
}

void Logger::shutdown() {
	std::lock_guard<std::mutex> lock(mutex);

	if (!initialized)
		return;

	if (file.is_open()) {
		writeSessionFooter();
		file.flush();
		file.close();
	}

	initialized = false;
}

void Logger::setLevel(LogLevel level) {
	std::lock_guard<std::mutex> lock(mutex);
	currentLevel = level;
}

LogLevel Logger::getLevel() const {
	std::lock_guard<std::mutex> lock(mutex);
	return currentLevel;
}

void Logger::setSDLMirrorEnabled(bool enabled) {
	std::lock_guard<std::mutex> lock(mutex);
	sdlMirror = enabled;
}

bool Logger::isSDLMirrorEnabled() const {
	std::lock_guard<std::mutex> lock(mutex);
	return sdlMirror;
}

bool Logger::isOpen() const {
	std::lock_guard<std::mutex> lock(mutex);
	return file.is_open();
}



void Logger::openFile() {
	namespace fs = std::filesystem;

	std::error_code ec;
	fs::create_directories(config.directory, ec);

	if (ec) {
		SDL_LogError(
			SDL_LOG_CATEGORY_APPLICATION,
			"Logger: failed to create log directory '%s': %s",
			config.directory.c_str(),
			ec.message().c_str()
		);
		return;
	}

	fs::path path = fs::path(config.directory) / config.filename;
	file.open(path, std::ios::out | std::ios::trunc);

	if (!file.is_open()) {
		SDL_LogError(
			SDL_LOG_CATEGORY_APPLICATION,
			"Logger: failed to open log file '%s'",
			path.string().c_str()
		);
	}
}

void Logger::writeSessionHeader() {
	auto now = std::chrono::system_clock::now();
	std::time_t t = std::chrono::system_clock::to_time_t(now);
	std::tm tm{};

	#ifdef _WIN32
		localtime_s(&tm, &t);
	#else
		localtime_r(&t, &tm);
	#endif

	char dateBuf[32];
	std::strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d %H:%M:%S", &tm);

	file << "================================================================\n"
		 << "  Blackthorn Engine  |  Session started: " << dateBuf << "\n"
		 << "================================================================\n";
	file.flush();
}

void Logger::writeSessionFooter() {
	file << "================================================================\n"
		 << "  Session ended\n"
		 << "================================================================\n";
}

std::string Logger::formatEntry(LogLevel level, std::string_view message, const char* srcFile, int srcLine) {
	auto now = std::chrono::system_clock::now();
	std::time_t t = std::chrono::system_clock::to_time_t(now);
	std::tm tm{};

	#ifdef _WIN32
		localtime_s(&tm, &t);
	#else
		localtime_r(&t, &tm);
	#endif

	char timeBuf[10]; // "HH:MM:SS\0"
	std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tm);

	std::string threadName = Threads::ThreadRegistry::instance().currentName();

	if (threadName.size() < 9) {
		threadName.resize(9, ' ');
	} else if (threadName.size() > 9) {
		threadName = threadName.substr(0, 9);
	}

	std::string entry = std::format("[{}] [{}] [{}] {}", timeBuf, levelTag(level), threadName, message);

	if (srcFile && srcLine > 0)
		entry += std::format("  ({}:{})", stripPath(srcFile), srcLine);

	return entry;
}

const char* Logger::levelTag(LogLevel level) {
	switch (level) {
		case LogLevel::Error:
			return "ERROR  ";
		case LogLevel::Warning:
			return "WARN   ";
		case LogLevel::Info:
			return "INFO   ";
		case LogLevel::Verbose:
			return "VERBOSE";
		case LogLevel::Debug:
			return "DEBUG  ";
		default:
			return "???????";
	}
}

const char* Logger::stripPath(const char* fullPath) {
	if (!fullPath)
		return "";

	const char* name = fullPath;

	for (const char* p = fullPath; *p != '\0'; ++p) {
		if (*p == '/' || *p == '\\')
			name = p + 1;
	}

	return name;
}

void Logger::forwardToSDL(LogLevel level, const char* entry) {
	switch (level) {
		case LogLevel::Error:
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", entry);
			break;
		case LogLevel::Warning:
			SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", entry);
			break;
		case LogLevel::Debug:
			SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "%s", entry);
			break;
		default:
			SDL_Log("%s", entry);
			break;
	}
}

void Logger::logImpl(LogLevel level, std::string_view message, const char* srcFile, int srcLine) {
	const bool mirror = sdlMirror.load(std::memory_order_relaxed);
	const std::string entry = formatEntry(level, message, srcFile, srcLine);

	{
		std::lock_guard<std::mutex> lock(mutex);

		if (file.is_open()) {
			file << entry << '\n';

			if (level <= LogLevel::Warning)
				file.flush();
		}
	}

	if (mirror)
		forwardToSDL(level, std::string(message).c_str());
}

} // namespace Blackthorn::Debug