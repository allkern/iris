// macOS shim for clock_nanosleep()/TIMER_ABSTIME

#ifndef IRIS_MACOS_CLOCK_NANOSLEEP_SHIM_H
#define IRIS_MACOS_CLOCK_NANOSLEEP_SHIM_H

#include <time.h>
#include <errno.h>

#ifndef TIMER_ABSTIME
#define TIMER_ABSTIME 1
#endif

static inline int clock_nanosleep(clockid_t clock_id, int flags, const struct timespec *request, struct timespec *remain)
{
	if (flags & TIMER_ABSTIME)
	{
		struct timespec shim_now;
		if (clock_gettime(clock_id, &shim_now) != 0)
			return errno;

		struct timespec shim_rel;
		shim_rel.tv_sec = request->tv_sec - shim_now.tv_sec;
		shim_rel.tv_nsec = request->tv_nsec - shim_now.tv_nsec;
		if (shim_rel.tv_nsec < 0)
		{
			shim_rel.tv_sec -= 1;
			shim_rel.tv_nsec += 1000000000L;
		}

		// Deadline already reached.
		if (shim_rel.tv_sec < 0 || (shim_rel.tv_sec == 0 && shim_rel.tv_nsec <= 0))
			return 0;

		if (nanosleep(&shim_rel, nullptr) != 0)
			return errno;
		return 0;
	}

	if (nanosleep(request, remain) != 0)
		return errno;
	return 0;
}

#endif
