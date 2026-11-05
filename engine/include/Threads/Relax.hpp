#pragma once

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
	#include <immintrin.h>
#elif defined(__arm__) || defined(_M_ARM) || defined(__aarch64__) || defined(_M_ARM64)
	#include <arm_acle.h>
#else
	#include <atomic>
#endif

namespace Blackthorn::Threads {

inline void relax() noexcept {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
	_mm_pause();
#elif defined(__arm__) || defined(_M_ARM) || defined(__aarch64__) || defined(_M_ARM64)
	__yield();
#else
	std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

} // namespace Blackthorn::Threads