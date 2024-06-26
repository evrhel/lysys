#ifndef _LS_TIME_H_
#define _LS_TIME_H_

struct ls_timespec
{
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
	int millisecond;
};

long long ls_nanotime(void);

double ls_time64(void);

float ls_time(void);

void ls_get_time(struct ls_timespec *ts);

void ls_get_local_time(struct ls_timespec *ts);

//! \brief Sleep for a number of milliseconds.
//! 
//! \param ms The minimum number of milliseconds to sleep.
void ls_sleep(unsigned long ms);

//! \brief Sleep for a number of nanoseconds.
//! 
//! \param ns The minimum number of nanoseconds to sleep.
void ls_nanosleep(long long ns);

#endif // _LYSYS_H_
