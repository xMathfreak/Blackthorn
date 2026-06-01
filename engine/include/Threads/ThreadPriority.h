#pragma once

#ifdef _WIN32
	#include <windows.h>
#endif

#include "Core/Export.h"
#include "Core/Types/Numeric.h"

namespace Blackthorn::Threads {

enum class ThreadPriority : U8 {
	Lowest,
	BelowNormal,
	Normal,
	AboveNormal,
	Highest,
};

bool BLACKTHORN_API setCurrentThreadPriority(ThreadPriority priority) noexcept;

bool BLACKTHORN_API setAudioThreadPriority() noexcept;

#if defined(_WIN32)
class BLACKTHORN_API MmcssScope {
public:
	MmcssScope();
	~MmcssScope();

	MmcssScope(const MmcssScope&) = delete;
	MmcssScope& operator=(const MmcssScope&) = delete;

	bool isValid() const noexcept { return handle != nullptr; }

private:
	HANDLE handle = nullptr;
	DWORD taskIndex = 0;
};
#endif

} // namespace Blackthorn::Threads