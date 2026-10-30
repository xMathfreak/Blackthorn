#pragma once

#include <cstdint>

namespace Blackthorn::Resources {

struct Resource {
	const char* name;
	const uint8_t* data = nullptr;
	size_t size = 0;

	explicit operator bool() const noexcept {
		return data != nullptr;
	}
};

const Resource* find(const char* name) noexcept;

size_t count() noexcept;

const Resource* get(size_t index) noexcept;

} // namespace Blackthorn::Resources