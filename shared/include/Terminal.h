#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#if defined(_WIN32)
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif
	#include <windows.h>
	#include <io.h>
#else
	#include <unistd.h>
#endif

namespace Blackthorn::Terminal {

namespace Detail {

/// @brief Platform-dispatching isatty(fileno(stream)) check.
inline bool isTerminal(std::FILE* stream) {
#if defined(_WIN32)
	return _isatty(_fileno(stream)) != 0;
#else
	return isatty(fileno(stream)) != 0;
#endif
}

#if defined(_WIN32)
/// @brief Windows-only: resolves the console handle matching `stream`.
inline HANDLE consoleHandleFor(std::FILE* stream) {
	return GetStdHandle(stream == stderr ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
}
#endif

/// @brief The uncached platform probe; supportsANSIColors() caches this per stream.
inline bool detectANSIColorSupport(std::FILE* stream) {
	// https://no-color.org
	if (std::getenv("NO_COLOR") != nullptr)
		return false;

	if (!isTerminal(stream))
		return false;

#if defined(_WIN32)
	HANDLE handle = consoleHandleFor(stream);
	if (handle == INVALID_HANDLE_VALUE)
		return false;

	DWORD dwMode = 0;
	if (!GetConsoleMode(handle, &dwMode))
		return false;

	if (dwMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)
		return true;

	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	return SetConsoleMode(handle, dwMode) != 0;
#else
	const char* term = std::getenv("TERM");
	if (!term)
		return false;

	const std::string termStr(term);
	if (termStr == "dumb")
		return false;

	static constexpr const char* knownColorTerms[] = {
		"color", "xterm", "linux", "screen", "tmux",
		"vt100", "rxvt", "konsole", "alacritty", "kitty"
	};

	for (const char* candidate : knownColorTerms) {
		if (termStr.find(candidate) != std::string::npos)
			return true;
	}

	return std::getenv("COLORTERM") != nullptr;
#endif
}

} // namespace Detail

/**
 * @brief True if `stream` is attached to a terminal, as opposed to a
 * redirected file or a pipe. Defaults to stdout.
 */
inline bool isTerminal(std::FILE* stream = stdout) {
	return Detail::isTerminal(stream);
}

/**
 * @brief True if writing ANSI color escape codes to `stream` is safe and
 * should render correctly. Defaults to stdout; pass stderr explicitly for
 * diagnostics printed there, since the two can legitimately disagree (one
 * redirected, the other not).
 *
 * Checks, in order: the NO_COLOR environment variable (always wins if
 * present), whether the stream is a terminal at all, then a platform-specific
 * capability check (`ENABLE_VIRTUAL_TERMINAL_PROCESSING` on Windows, a
 * `TERM`/`COLORTERM` heuristic on POSIX).
 *
 * The result is cached per stream after its first call.
 */
inline bool supportsANSIColors(std::FILE* stream = stdout) {
	static const bool cachedStdout = Detail::detectANSIColorSupport(stdout);
	static const bool cachedStderr = Detail::detectANSIColorSupport(stderr);
	return stream == stderr ? cachedStderr : cachedStdout;
}

/// @brief A small, terminal-portable color palette.
enum class Color {
	Reset,
	Bold,
	Red,
	Green,
	Yellow,
	Blue,
	Magenta,
	Cyan,
};

namespace Detail {

inline const char* ansiCode(Color color) {
	switch (color) {
		case Color::Reset:   return "\x1b[0m";
		case Color::Bold:    return "\x1b[1m";
		case Color::Red:     return "\x1b[31m";
		case Color::Green:   return "\x1b[32m";
		case Color::Yellow:  return "\x1b[33m";
		case Color::Blue:    return "\x1b[34m";
		case Color::Magenta: return "\x1b[35m";
		case Color::Cyan:    return "\x1b[36m";
	}
	return "";
}

} // namespace Detail

/**
 * @brief Wraps `text` in the ANSI escape codes for `color`, or returns it
 * unchanged if `stream` doesn't support color (see supportsANSIColors()).
 * `stream` should match wherever the result is actually going to be printed.
 */
inline std::string colorize(std::string_view text, Color color, std::FILE* stream = stdout) {
	if (!supportsANSIColors(stream))
		return std::string(text);
	return std::string(Detail::ansiCode(color)) + std::string(text) + Detail::ansiCode(Color::Reset);
}

} // namespace Blackthorn::Terminal
