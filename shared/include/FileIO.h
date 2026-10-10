#pragma once

#include <cstdint>
#include <cstdio>

namespace Blackthorn {

/**
 * @brief Seeks an open FILE* to a byte offset relative to @p origin.
 * @return true on success.
 */
inline bool seekTo(std::FILE* f, uint64_t offset, int origin = SEEK_SET) {
#ifdef _WIN32
	return _fseeki64(f, static_cast<__int64>(offset), origin) == 0;
#else
	return std::fseek(f, static_cast<long>(offset), origin) == 0;
#endif
}

/**
 * @brief Returns the current byte position of an open FILE*, or -1 on failure.
 */
inline int64_t fileTell(std::FILE* f) {
#ifdef _WIN32
	return _ftelli64(f);
#else
	return static_cast<int64_t>(std::ftell(f));
#endif
}

/** @brief Reads exactly @p count bytes. Returns false on a short read. */
inline bool readExact(std::FILE* f, void* dst, size_t count) {
	return std::fread(dst, 1, count, f) == count;
}

/** @brief Writes exactly @p count bytes. Returns false on error. */
inline bool writeExact(std::FILE* f, const void* src, size_t count) {
	return std::fwrite(src, 1, count, f) == count;
}

} // namespace Blackthorn
