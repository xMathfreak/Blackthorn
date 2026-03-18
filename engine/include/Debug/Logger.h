#pragma once

#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

#include "Core/Export.h"

namespace Blackthorn::Debug {

/**
 * @brief Controls which messages the Logger writes.
 *
 * Ordered by verbosity — a message is emitted only when its level is <=
 * the currently configured level:
 *
 *   Silent   — Nothing is written.
 *   Error    — Only errors.
 *   Warning  — Errors + warnings.
 *   Info — Errors + warnings + informational messages  (Release default).
 *   Verbose  — All of the above + verbose trace detail.
 *   Debug    — Everything, including fine-grained debug output  (Debug default).
 */
enum class LogLevel : int {
	Silent   = 0,
	Error    = 1,
	Warning  = 2,
	Info     = 3,
	Verbose  = 4,
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
 * @brief Thread-safe file logger with configurable log levels and an optional
 *        SDL_Log mirror.
 *
 * Lifecycle
 * ---------
 *
 *   `Logger::instance().init(config);`   // once, inside Engine::init()
 *   `BT_LOG("Engine started");`
 *   `Logger::instance().shutdown();`     // once, at the tail of Engine::shutdown()
 *
 * The log file is opened in truncate mode on init() — previous contents are
 * discarded on every new run.
 *
 * SDL mirror
 * ----------
 *
 * When enabled, every entry is also forwarded to the appropriate SDL log
 * function. This can be toggled at any time:
 *
 *   `Logger::instance().setSDLMirrorEnabled(false);`
 *
 * Log entry format
 * ----------------
 *
 *   [HH:MM:SS] [LEVEL  ] message  (filename:line)
 *
 * The source location suffix is only appended when the BT_* macros are used
 * (they inject __FILE__ and __LINE__ automatically).
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
	 * @brief Write a message at the given level.
	 *
	 * Filtered out immediately (before acquiring any lock) when level is
	 * greater than the current log level, making disabled levels near-zero cost.
	 *
	 * Prefer the BT_* macros so that source file and line are captured.
	 *
	 * @param level   Severity of this message.
	 * @param message Text to write.
	 * @param srcFile Source file (__FILE__), or nullptr to omit the location.
	 * @param srcLine Source line (__LINE__), ignored when srcFile is nullptr.
	 */
	void log(
		LogLevel         level,
		std::string_view message,
		const char*      srcFile = nullptr,
		int              srcLine = 0
	);

	void info    (std::string_view msg, const char* f = nullptr, int line = 0) { log(LogLevel::Info, msg, f, line); }
	void warn    (std::string_view msg, const char* f = nullptr, int line = 0) { log(LogLevel::Warning,  msg, f, line); }
	void error   (std::string_view msg, const char* f = nullptr, int line = 0) { log(LogLevel::Error,    msg, f, line); }
	void verbose (std::string_view msg, const char* f = nullptr, int line = 0) { log(LogLevel::Verbose,  msg, f, line); }
	void debug   (std::string_view msg, const char* f = nullptr, int line = 0) { log(LogLevel::Debug,    msg, f, line); }

private:
	Logger();
	~Logger();

	/// Open (or re-open) the log file, creating the directory if needed.
	void openFile();

	/// Write a formatted session divider + timestamp line to the open file.
	void writeSessionHeader();
	void writeSessionFooter();

	/// Build a complete, formatted log entry string.
	static std::string formatEntry(
		LogLevel         level,
		std::string_view message,
		const char*      srcFile,
		int              srcLine
	);

	/// Return just the filename portion of a full source path.
	static const char* stripPath(const char* fullPath);

	/// Fixed-width label for each log level.
	static const char* levelTag(LogLevel level);

	/// Forward a pre-formatted entry to the appropriate SDL log function.
	static void forwardToSDL(LogLevel level, const char* entry);

	mutable std::mutex mutex;

	std::ofstream file;
	LoggerConfig  config;

	LogLevel currentLevel   = LogLevel::Info;
	bool     sdlMirror      = true;
	bool     initialized    = false;
};

} // namespace Blackthorn::Debug

// Convenience macros
//
// Accept any expression convertible to std::string_view. For formatted output,
// build the string beforehand:
//
//   BT_LOG("Loaded " + std::to_string(count) + " assets");
//   BT_ERROR(std::format("Failed to open '{}'", path));

#ifdef BLACKTHORN_DEBUG
	#define BT_LOG(msg)     ::Blackthorn::Debug::Logger::instance().info((msg), __FILE__, __LINE__)
	#define BT_WARN(msg)    ::Blackthorn::Debug::Logger::instance().warn    ((msg), __FILE__, __LINE__)
	#define BT_ERROR(msg)   ::Blackthorn::Debug::Logger::instance().error   ((msg), __FILE__, __LINE__)
	#define BT_VERBOSE(msg) ::Blackthorn::Debug::Logger::instance().verbose ((msg), __FILE__, __LINE__)
	#define BT_DEBUG(msg)   ::Blackthorn::Debug::Logger::instance().debug   ((msg), __FILE__, __LINE__)
#else
	#define BT_LOG(msg)     ::Blackthorn::Debug::Logger::instance().info((msg))
	#define BT_WARN(msg)    ::Blackthorn::Debug::Logger::instance().warn    ((msg))
	#define BT_ERROR(msg)   ::Blackthorn::Debug::Logger::instance().error   ((msg))
	#define BT_VERBOSE(msg) ::Blackthorn::Debug::Logger::instance().verbose ((msg))
	#define BT_DEBUG(msg)   ::Blackthorn::Debug::Logger::instance().debug   ((msg))
#endif