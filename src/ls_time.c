#include "ls_native.h"

#include <time.h>

#include <lysys/ls_time.h>
#include <lysys/ls_defs.h>

#if LS_WINDOWS
static LARGE_INTEGER _li_freq = { .QuadPart = 0 };
static LARGE_INTEGER _li_start = { .QuadPart = 0 };
#else
static struct timespec _ts_start = { .tv_sec = 0, .tv_nsec = 0 };
#endif // LS_WINDOWS

void ls_set_epoch(void)
{
#if LS_WINDOWS
	QueryPerformanceFrequency(&_li_freq);
	QueryPerformanceCounter(&_li_start);
#else
	clock_gettime(CLOCK_MONOTONIC, &_ts_start);
#endif // LS_WINDOWS
}

long long ls_nano_time(void)
{
#if LS_WINDOWS
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return (li.QuadPart - _li_start.QuadPart) * 1000000000LL / _li_freq.QuadPart;
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (ts.tv_sec - _ts_start.tv_sec) * 1000000000LL + ts.tv_nsec - _ts_start.tv_nsec;
#endif // LS_WINDOWS
}

double ls_time64(void)
{
#if LS_WINDOWS
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return (double)(li.QuadPart - _li_start.QuadPart) / _li_freq.QuadPart;
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)(ts.tv_sec - _ts_start.tv_sec) + (double)ts.tv_nsec / 1000000000.0;
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
#endif //

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

	QueryPerformanceCounter(&li_start);

	i64_sleep_time = ns / 1000000LL - 1;
	if (i64_sleep_time > 0)
		Sleep((DWORD)i64_sleep_time);

	QueryPerformanceCounter(&li_now);

	ns -= (li_now.QuadPart - li_start.QuadPart) * 1000000000LL / _li_freq.QuadPart;

	i64_end = li_now.QuadPart + ns * _li_freq.QuadPart / 1000000000LL;

	while (li_now.QuadPart < i64_end)
		QueryPerformanceCounter(&li_now);
#else
	struct timespec ts;

	ts.tv_sec = ns / 1000000000LL;
	ts.tv_nsec = ns % 1000000000LL;

	ls_sleep_ts(&ts);
#endif // LS_WINDOWS
}
