#include "Threads/ThreadPriority.h"

#if defined(_WIN32)
	#include <windows.h>
	#include <avrt.h>
#elif defined (__APPLE__)
	#include <pthread.h>
#elif defined(__linux__)
	#include <pthread.h>
	#include <sched.h>
	#include <errno.h>
	#include <string.h>
	#include <sys/resource.h>
#endif

namespace Blackthorn::Threads {

bool setCurrentThreadPriority(ThreadPriority priority) noexcept
{
#if defined(_WIN32)

	int winPriority = THREAD_PRIORITY_NORMAL;

	switch (priority)
	{
	case ThreadPriority::Lowest:
		winPriority = THREAD_PRIORITY_LOWEST;
		break;

	case ThreadPriority::BelowNormal:
		winPriority = THREAD_PRIORITY_BELOW_NORMAL;
		break;

	case ThreadPriority::Normal:
		winPriority = THREAD_PRIORITY_NORMAL;
		break;

	case ThreadPriority::AboveNormal:
		winPriority = THREAD_PRIORITY_ABOVE_NORMAL;
		break;

	case ThreadPriority::Highest:
		winPriority = THREAD_PRIORITY_HIGHEST;
		break;
	}

	return SetThreadPriority(GetCurrentThread(), winPriority) == TRUE;


#elif defined(__APPLE__)

	qos_class_t qos = QOS_CLASS_DEFAULT;

	switch (priority)
	{
	case ThreadPriority::Lowest:
		qos = QOS_CLASS_BACKGROUND;
		break;

	case ThreadPriority::BelowNormal:
		qos = QOS_CLASS_UTILITY;
		break;

	case ThreadPriority::Normal:
		qos = QOS_CLASS_DEFAULT;
		break;

	case ThreadPriority::AboveNormal:
		qos = QOS_CLASS_USER_INITIATED;
		break;

	case ThreadPriority::Highest:
		qos = QOS_CLASS_USER_INTERACTIVE;
		break;
	}

	return pthread_set_qos_class_self_np(qos, 0) == 0;

#elif defined(__linux__)
	int niceValue = 0;

	switch (priority)
	{
	case ThreadPriority::Lowest:
		niceValue = 10;
		break;

	case ThreadPriority::BelowNormal:
		niceValue = 5;
		break;

	case ThreadPriority::Normal:
		niceValue = 0;
		break;

	case ThreadPriority::AboveNormal:
		niceValue = -5;
		break;

	case ThreadPriority::Highest:
		niceValue = -10;
		break;
	}

	if (setpriority(PRIO_PROCESS, 0, niceValue) != 0) {
		return false;
	}

	return true;


#else

	(void)priority;
	return true;

#endif
}

bool setAudioThreadPriority() noexcept {
#if defined(_WIN32)
	static thread_local MmcssScope scope;
	return scope.isValid();
#elif defined(__APPLE__)
	int rc = pthread_set_qos_class_self_np(
		QOS_CLASS_USER_INITIATED,
		0
	);

	return rc == 0;
#elif defined(__linux__)
	sched_param param{};
	param.sched_priority = sched_get_priority_max(SCHED_RR) / 2;

	int rc = pthread_setschedparam(
		pthread_self(),
		SCHED_RR,
		&param
	);

	return rc == 0;
#else
	return true;
#endif
}

#ifdef _WIN32
MmcssScope::MmcssScope() {
	handle = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
}

MmcssScope::~MmcssScope() {
	if (handle)
		AvRevertMmThreadCharacteristics(handle);
}
#endif

} // namespace Blackthorn::Threads