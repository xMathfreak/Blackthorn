#pragma once

#include <algorithm>
#include <cstring>
#include <format>
#include <string_view>

namespace Blackthorn::Localization {

/**
 * @brief Small-buffer-optimized, null-terminated string for formatted
 *        localized text.
 *
 * @details Holds up to `InlineCapacity - 1` bytes inline (no allocation);
 *          longer results transparently spill onto the heap. Sized larger
 *          than std::string's own SSO buffer because localized dialogue and
 *          UI strings routinely exceed the ~15-22 bytes most std::string
 *          implementations inline.
 *
 * @note This is an append-only builder, not a general-purpose string type:
 *       it is built once by LocalizationManager::format() and then read via
 *       view()/c_str(). It intentionally does not support mid-string
 *       insertion, erase, or other std::string operations.
 */
class LocalizedString {
public:
	LocalizedString() noexcept {
		storage.inlineBuf[0] = '\0';
	}

	LocalizedString(const LocalizedString& other) {
		storage.inlineBuf[0] = '\0';
		append(other.view());
	}

	LocalizedString(LocalizedString&& other) noexcept {
		moveFrom(other);
	}

	LocalizedString& operator=(const LocalizedString& other) {
		if (this != &other) {
			release();
			len = 0;
			heapActive = false;
			storage.inlineBuf[0] = '\0';
			append(other.view());
		}
		return *this;
	}

	LocalizedString& operator=(LocalizedString&& other) noexcept {
		if (this != &other) {
			release();
			moveFrom(other);
		}
		return *this;
	}

	~LocalizedString() {
		release();
	}

	/// @brief Appends text to the end of the string, growing (and moving to
	///        the heap, if necessary) to fit.
	void append(std::string_view text) {
		if (text.empty())
			return;

		const size_t newLen = len + text.size();

		if (!heapActive && newLen + 1 <= InlineCapacity) {
			std::memcpy(storage.inlineBuf + len, text.data(), text.size());
			len = newLen;
			storage.inlineBuf[len] = '\0';
			return;
		}

		if (!heapActive) {
			// Migrating from inline storage to the heap.
			const size_t newCap = std::max(newLen + 1, InlineCapacity * 2);
			char* buf = new char[newCap];
			std::memcpy(buf, storage.inlineBuf, len);
			std::memcpy(buf + len, text.data(), text.size());
			len = newLen;
			buf[len] = '\0';
			storage.heap.data = buf;
			storage.heap.capacity = newCap;
			heapActive = true;
			return;
		}

		if (newLen + 1 > storage.heap.capacity) {
			size_t newCap = storage.heap.capacity * 2;
			newCap = std::max(newCap, newLen + 1);
			char* buf = new char[newCap];
			std::memcpy(buf, storage.heap.data, len);
			delete[] storage.heap.data;
			storage.heap.data = buf;
			storage.heap.capacity = newCap;
		}

		std::memcpy(storage.heap.data + len, text.data(), text.size());
		len = newLen;
		storage.heap.data[len] = '\0';
	}

	[[nodiscard]] std::string_view view() const noexcept { return { data(), len }; }
	[[nodiscard]] const char* c_str() const noexcept { return data(); }
	[[nodiscard]] size_t size() const noexcept { return len; }
	[[nodiscard]] bool empty() const noexcept { return len == 0; }
	[[nodiscard]] bool isOnHeap() const noexcept { return heapActive; }

	operator std::string_view() const noexcept { return view(); }

private:
	// Total inline buffer size in bytes, including the null terminator, so
	// the longest string stored inline without allocating
	// is `InlineCapacity - 1` characters.
	static constexpr size_t InlineCapacity = 48;

	union Storage {
		char inlineBuf[InlineCapacity];

		struct {
			char* data;
			size_t capacity;
		} heap;

		Storage() {}
	} storage;

	size_t len = 0;
	bool heapActive = false;

	char* data() noexcept { return heapActive ? storage.heap.data : storage.inlineBuf; }
	const char* data() const noexcept { return heapActive ? storage.heap.data : storage.inlineBuf; }

	void moveFrom(LocalizedString& other) noexcept {
		heapActive = other.heapActive;
		len = other.len;

		if (heapActive) {
			storage.heap = other.storage.heap;
			other.heapActive = false;
			other.len = 0;
			other.storage.inlineBuf[0] = '\0';
		} else {
			std::memcpy(storage.inlineBuf, other.storage.inlineBuf, len + 1);
		}
	}

	void release() noexcept {
		if (heapActive) {
			delete[] storage.heap.data;
			heapActive = false;
		}
	}
};

} // namespace Blackthorn::Localization

namespace std {

/// @brief Lets LocalizedString be passed directly to std::format/std::vformat
///        (e.g. BT_LOG("{}", locStr)) by delegating to the string_view
///        formatter over its contents.
template<>
struct formatter<Blackthorn::Localization::LocalizedString> : formatter<std::string_view> {
	auto format(const Blackthorn::Localization::LocalizedString& s, format_context& ctx) const {
		return formatter<std::string_view>::format(s.view(), ctx);
	}
};

} // namespace std
