#pragma once

#include <atomic>
#include <format>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

#include "Core/Export.h"

namespace Blackthorn::Debug {

/**
 * @brief Controls which messages the Logger writes.
 *
 * Ordered by verbosity - a message is emitted only when its level is <=
 * the currently configured level:
 *
 *   Silent   - Nothing is written.
 *   Error    - Only errors.
 *   Warning  - Errors + warnings.
 *   Info     - Errors + warnings + informational messages  (Release default).
 *   Trace  - All of the above + verbose trace detail.
 *   Debug    - Everything, including fine-grained debug output  (Debug default).
 */
enum class LogLevel : int {
	Silent   = 0,
	Error    = 1,
	Warning  = 2,
	Info     = 3,
	Trace    = 4,
	Debug    = 5,
};

/**
 * @brief Configuration passed to `Logger::init()`.
 *
 * Embed this inside EngineConfig::debug so it is provided alongside the rest
 * of the engine configuration.
 */
struct BLACKTHORN_API LoggerConfig {
	/// Directory that will hold the log file (created automatically if absent).
	std::string directory = "logs";

	/// Log filename including extension, e.g. "blackthorn.log".
	std::string filename = "blackthorn.log";

	/// Whether to mirror log entries to `SDL_Log` / `SDL_LogWarn` / `SDL_LogError`
	/// at startup.
	/// Can be toggled at any time via `Logger::setSDLMirrorEnabled()`.
	#ifdef BLACKTHORN_DEBUG
		bool mirrorToSDL = true;
	#else
		bool mirrorToSDL = false;
	#endif
};

/**
 * @brief Thread-safe file logger with compile-time validated format strings.
 *
 * @section lifecycle Lifecycle
 * @code
 * Logger::instance().init(config);
 * BT_LOG("Engine started");
 * Logger::instance().shutdown();
 * @endcode
 *
 * @section format Log entry format
 * @code
 * [HH:MM:SS] [LEVEL  ] [ThreadName] message  (filename:line)
 * @endcode
 *
 * The source location suffix is only appended in Debug builds via the BT_*
 * macros, which inject __FILE__ and __LINE__ automatically.
 *
 * @section runtime_strings Runtime strings
 * Because format strings must be compile-time constants, a pre-built
 * std::string must be passed through a format specifier:
 *
 * @code
 * std::string msg = buildMessage();
 * BT_ERROR("{}", msg);
 * @endcode
 */
class BLACKTHORN_API Logger {
public:
	static Logger& instance();

	Logger(const Logger&) = delete;
	Logger& operator=(const Logger&) = delete;

	/**
	 * @brief Opens the log file in truncate mode and writes a session header.
	 *
	 * Safe to call more than once; each call closes the previous file,
	 * re-opens, and writes a fresh header.
	 *
	 * The log level is set automatically based on build configuration:
	 *   Debug build   → `LogLevel::Debug`.
	 *   Release build → `LogLevel::Info`.
	 * Override afterwards with setLevel() if needed.
	 *
	 * @param config Logger configuration (directory, filename, SDL mirror flag).
	 */
	void init(const LoggerConfig& config = LoggerConfig());

	/**
	 * @brief Flushes, writes a session footer, and closes the log file.
	 */
	void shutdown();

	/**
	 * @brief Set the minimum level required for a message to be written.
	 *
	 * Thread-safe. Takes effect for all subsequent `log()` calls.
	 */
	void setLevel(LogLevel level);

	/**
	 * @brief Returns the current log level.
	 */
	LogLevel getLevel() const;

	/**
	 * @brief Enable or disable the SDL_Log mirror at runtime.
	 *
	 * Thread-safe. Useful for redirecting output during e.g. a replay or test.
	 */
	void setSDLMirrorEnabled(bool enabled);

	/**
	 * @brief Returns whether the SDL mirror is currently active.
	 */
	bool isSDLMirrorEnabled() const;

	/**
	 * @brief Returns true if the log file is open and ready for writing.
	 */
	bool isOpen() const;

	/**
	 * @brief Formats and logs a message at the given level, with source location.
	 *
	 * Intended to be called via the BT_* macros, which inject __FILE__
	 * and __LINE__ automatically.
	 *
	 * @param level Severity level.
	 * @param srcFile Source file (__FILE__).
	 * @param srcLine Source line (__LINE__).
	 * @param fmt  Compile-time validated format string.
	 * @param args Format arguments.
	 */
	template <typename... Args>
	void log(LogLevel level, const char* srcFile, int srcLine, std::format_string<Args...> fmt, Args&&... args) {
		if (static_cast<int>(level) > static_cast<int>(currentLevel.load(std::memory_order::relaxed)))
			return;

		logImpl(level, std::format(fmt, std::forward<Args>(args)...), srcFile, srcLine);
	}

	template <typename... Args>
	void info(std::format_string<Args...> fmt, Args&&... args) {
		log(LogLevel::Info, fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void warn(std::format_string<Args...> fmt, Args&&... args) {
		log(LogLevel::Warning, fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void error(std::format_string<Args...> fmt, Args&&... args) {
		log(LogLevel::Error, fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void trace(std::format_string<Args...> fmt, Args&&... args) {
		log(LogLevel::Trace, fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void debug(std::format_string<Args...> fmt, Args&&... args) {
		log(LogLevel::Debug, fmt, std::forward<Args>(args)...);
	}
private:
	Logger();
	~Logger();

	/// Open (or re-open) the log file, creating the directory if needed.
	void openFile();

	/// Build a complete, formatted log entry string.
	static std::string formatEntry(
		LogLevel         level,
		std::string_view message,
		const char*      srcFile,
		int              srcLine
	);

	/// Performs the actual write.
	void logImpl(LogLevel level, std::string_view message, const char* srcFile, int srcLine);

	/// Return just the filename portion of a full source path.
	static const char* stripPath(const char* fullPath);

	/// Fixed-width label for each log level.
	static const char* levelTag(LogLevel level);

	/// Forward a pre-formatted entry to the appropriate SDL log function.
	static void forwardToSDL(LogLevel level, const char* entry);

	mutable std::mutex mutex;

	std::ofstream file;
	LoggerConfig  config;

	std::atomic<LogLevel> currentLevel{LogLevel::Info};
	std::atomic<bool> sdlMirror {true};
	bool initialized = false;
};

} // namespace Blackthorn::Debug

// Convenience macros
//
// Accept any expression convertible to std::string_view. For formatted output,
// build the string beforehand:
//
//   BT_LOG("Loaded " + std::to_string(count) + " assets");
//   BT_ERROR("Failed to open '{}'", path);


#ifdef BLACKTHORN_DEBUG
	#define BT_LOG(...)     ::Blackthorn::Debug::Logger::instance().log(::Blackthorn::Debug::LogLevel::Info,    __FILE__, __LINE__, __VA_ARGS__)
	#define BT_WARN(...)    ::Blackthorn::Debug::Logger::instance().log(::Blackthorn::Debug::LogLevel::Warning, __FILE__, __LINE__, __VA_ARGS__)
	#define BT_ERROR(...)   ::Blackthorn::Debug::Logger::instance().log(::Blackthorn::Debug::LogLevel::Error,   __FILE__, __LINE__, __VA_ARGS__)
	#define BT_TRACE(...) ::Blackthorn::Debug::Logger::instance().log(::Blackthorn::Debug::LogLevel::Trace, __FILE__, __LINE__, __VA_ARGS__)
	#define BT_DEBUG(...)   ::Blackthorn::Debug::Logger::instance().log(::Blackthorn::Debug::LogLevel::Debug,   __FILE__, __LINE__, __VA_ARGS__)
#else
	#define BT_LOG(...)     ::Blackthorn::Debug::Logger::instance().log(::Blackthorn::Debug::LogLevel::Info,    nullptr, 0, __VA_ARGS__)
	#define BT_WARN(...)    ::Blackthorn::Debug::Logger::instance().log(::Blackthorn::Debug::LogLevel::Warning, nullptr, 0, __VA_ARGS__)
	#define BT_ERROR(...)   ::Blackthorn::Debug::Logger::instance().log(::Blackthorn::Debug::LogLevel::Error,   nullptr, 0, __VA_ARGS__)
	#define BT_TRACE(...) ::Blackthorn::Debug::Logger::instance().log(::Blackthorn::Debug::LogLevel::Trace, nullptr, 0, __VA_ARGS__)
	#define BT_DEBUG(...)   ::Blackthorn::Debug::Logger::instance().log(::Blackthorn::Debug::LogLevel::Debug,   nullptr, 0, __VA_ARGS__)
#endif