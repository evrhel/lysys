#include <lysys/ls_time.h>

#include <time.h>
#include <math.h>

#include <lysys/ls_time.h>
#include <lysys/ls_defs.h>

#include "ls_native.h"

#define NS_PER_SEC 1000000000LL
#define NS_PER_MS 1000000LL

#if LS_WINDOWS
static LARGE_INTEGER li_freq = { 0 };
#endif // LS_WINDOWS

long long ls_nanotime(void)
{
#if LS_WINDOWS
	LARGE_INTEGER li;
	if (li_freq.QuadPart == 0)
		(void)QueryPerformanceFrequency(&li_freq);
	(void)QueryPerformanceCounter(&li);
	return li.QuadPart * (NS_PER_SEC / li_freq.QuadPart);
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * NS_PER_SEC + ts.tv_nsec;
#endif // LS_WINDOWS
}

double ls_time64(void)
{
#if LS_WINDOWS
	LARGE_INTEGER li;
	if (li_freq.QuadPart == 0)
		(void)QueryPerformanceFrequency(&li_freq);
	(void)QueryPerformanceCounter(&li);
	return (double)li.QuadPart / li_freq.QuadPart;
#else
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec / NS_PER_SEC;
#endif // LS_WINDOWS
}

float ls_time(void) { return (float)ls_time64(); }

void ls_get_time(struct ls_timespec *ts)
{
#if LS_WINDOWS
	SYSTEMTIME st;
	GetSystemTime(&st);

	ts->year = st.wYear;
	ts->month = st.wMonth;
	ts->day = st.wDay;
	ts->hour = st.wHour;
	ts->minute = st.wMinute;
	ts->second = st.wSecond;
	ts->millisecond = st.wMilliseconds;
#else
	time_t t;
	struct tm *tm;

	t = time(NULL);
	tm = gmtime(&t);

	ts->year = tm->tm_year + 1900;
	ts->month = tm->tm_mon + 1;
	ts->day = tm->tm_mday;
	ts->hour = tm->tm_hour;
	ts->minute = tm->tm_min;
	ts->second = tm->tm_sec;
	ts->millisecond = 0;
#endif // LS_WINDOWS
}

void ls_get_local_time(struct ls_timespec *ts)
{
#if LS_WINDOWS
	SYSTEMTIME st;
	GetLocalTime(&st);

	ts->year = st.wYear;
	ts->month = st.wMonth;
	ts->day = st.wDay;
	ts->hour = st.wHour;
	ts->minute = st.wMinute;
	ts->second = st.wSecond;
	ts->millisecond = st.wMilliseconds;
#else
	time_t t;
	struct tm *tm;

	t = time(NULL);
	tm = localtime(&t);

	ts->year = tm->tm_year + 1900;
	ts->month = tm->tm_mon + 1;
	ts->day = tm->tm_mday;
	ts->hour = tm->tm_hour;
	ts->minute = tm->tm_min;
	ts->second = tm->tm_sec;
	ts->millisecond = 0;
#endif // LS_WINDOWS
}

#if LS_POSIX
static void ls_sleep_ts(struct timespec *ts)
{
	struct timespec rem;
	int rc;

	for (;;)
	{
		rc = nanosleep(ts, &rem);
		if (rc == 0)
			break;
		
		if (errno != EINTR)
			break; // error

		*ts = rem;
	}
}
#endif // LS_POSIX

void ls_sleep(unsigned long ms)
{
#if LS_WINDOWS
	Sleep(ms);
#else
	struct timespec ts;

	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000;

	ls_sleep_ts(&ts);
#endif // LS_WINDOWS
}

void ls_nanosleep(long long ns)
{
#if LS_WINDOWS
	LARGE_INTEGER li_start, li_now;
	__int64 i64_end, i64_sleep_time;

	if (li_freq.QuadPart == 0)
		(void)QueryPerformanceFrequency(&li_freq);

	(void)QueryPerformanceCounter(&li_start);

	i64_sleep_time = ns / NS_PER_MS - 1;
	if (i64_sleep_time > 0)
		Sleep((DWORD)i64_sleep_time);

	(void)QueryPerformanceCounter(&li_now);

	ns -= (li_now.QuadPart - li_start.QuadPart) * (NS_PER_SEC / li_freq.QuadPart);

	i64_end = li_now.QuadPart + ns * (li_freq.QuadPart / NS_PER_SEC);

	while (li_now.QuadPart < i64_end)
		(void)QueryPerformanceCounter(&li_now);
#else
	struct timespec ts;

	ts.tv_sec = ns / NS_PER_SEC;
	ts.tv_nsec = ns % NS_PER_SEC;

	ls_sleep_ts(&ts);
#endif // LS_WINDOWS
}
