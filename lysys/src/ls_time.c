#include "ls_native.h"

#include <lysys/ls_time.h>
#include <lysys/ls_defs.h>

#if LS_WINDOWS
static LARGE_INTEGER _li_freq;
static LARGE_INTEGER _li_start;
#endif

void ls_set_epoch(void)
{
#if LS_WINDOWS
	QueryPerformanceFrequency(&_li_freq);
	QueryPerformanceCounter(&_li_start);
#endif
}

long long ls_nano_time(void)
{
#if LS_WINDOWS
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return (li.QuadPart - _li_start.QuadPart) * 1000000000LL / _li_freq.QuadPart;
#endif
}

double ls_time64(void)
{
#if LS_WINDOWS
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return (double)(li.QuadPart - _li_start.QuadPart) / _li_freq.QuadPart;
#endif
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
#endif
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
#endif
}

void ls_sleep(unsigned long ms)
{
#if LS_WINDOWS
	Sleep(ms);
#endif
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
#endif
}
