#pragma once

#include <cstdint>
#include <limits>

using U8  = uint8_t;
using I8  = int8_t;

using U16 = uint16_t;
using I16 = int16_t;

using U32 = uint32_t;
using I32 = int32_t;

using U64 = uint64_t;
using I64 = int64_t;

using UMAX = uintmax_t;
using IMAX = intmax_t;

using F32 = float;
using F64 = double;

constexpr U8  U8_MAX  = std::numeric_limits<U8>::max();
constexpr I8  I8_MAX  = std::numeric_limits<I8>::max();
constexpr I8  I8_MIN  = std::numeric_limits<I8>::min();

constexpr U16 U16_MAX = std::numeric_limits<U16>::max();
constexpr I16 I16_MAX = std::numeric_limits<I16>::max();
constexpr I16 I16_MIN = std::numeric_limits<I16>::min();

constexpr U32 U32_MAX = std::numeric_limits<U32>::max();
constexpr I32 I32_MAX = std::numeric_limits<I32>::max();
constexpr I32 I32_MIN = std::numeric_limits<I32>::min();

constexpr U64 U64_MAX = std::numeric_limits<U64>::max();
constexpr I64 I64_MAX = std::numeric_limits<I64>::max();
constexpr I64 I64_MIN = std::numeric_limits<I64>::min();

constexpr UMAX UMAX_MAX = std::numeric_limits<UMAX>::max();
constexpr IMAX IMAX_MAX = std::numeric_limits<IMAX>::max();
constexpr IMAX IMAX_MIN = std::numeric_limits<IMAX>::min();