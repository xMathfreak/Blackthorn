#pragma once

#include <cstdint>

using U8  = uint8_t;
using I8  = int8_t;

using U16 = uint16_t;
using I16 = int16_t;

using U32 = uint32_t;
using I32 = int32_t;

using U64 = uint64_t;
using I64 = int64_t;

using F32 = float;
using F64 = double;

using B8 = bool;

constexpr U8  U8_MAX  = UINT8_MAX;
constexpr I8  I8_MAX  = INT8_MAX;
constexpr I8  I8_MIN  = INT8_MIN;

constexpr U16 U16_MAX = UINT16_MAX;
constexpr I16 I16_MAX = INT16_MAX;
constexpr I16 I16_MIN = INT16_MIN;

constexpr U32 U32_MAX = UINT32_MAX;
constexpr I32 I32_MAX = INT32_MAX;
constexpr I32 I32_MIN = INT32_MIN;

constexpr U64 U64_MAX = UINT64_MAX;
constexpr I64 I64_MAX = INT64_MAX;
constexpr I64 I64_MIN = INT64_MIN;