#pragma once

#include <string>
#include <utility>
#include <variant>

namespace Blackthorn::Scripting {

/**
 * @brief Describes a failed Lua<->C++ operation.
 */
struct LuaError {
	std::string message;     ///< Human-readable text, safe to log or raise.
	int argIndex = 0;        ///< 1-based stack index of the offending value, 0 if n/a.
};

/**
 * @brief Value-or-error result used at the host boundary and in converters.
 * @tparam T The success type.
 */
template <typename T>
class LuaResult {
public:
	LuaResult(T value) : storage_(std::in_place_index<0>, std::move(value)) {}
	LuaResult(LuaError error) : storage_(std::in_place_index<1>, std::move(error)) {}

	[[nodiscard]] bool ok() const noexcept { return storage_.index() == 0; }
	[[nodiscard]] T& value() { return std::get<0>(storage_); }
	[[nodiscard]] LuaError& error() { return std::get<1>(storage_); }

private:
	std::variant<T, LuaError> storage_;
};

/** @brief void specialization of LuaResult. */
template <>
class LuaResult<void> {
public:
	LuaResult() = default;
	LuaResult(LuaError error) : error_(std::move(error)), failed_(true) {}

	[[nodiscard]] bool ok() const noexcept { return !failed_; }
	[[nodiscard]] LuaError& error() { return error_; }

private:
	LuaError error_{};
	bool failed_ = false;
};

} // namespace Blackthorn::Scripting