#ifndef _LS_THREADDEF_H_
#define	_LS_THREADDEF_H_

#include "ls_native.h"

struct ls_thread
{
#if LS_WINDOWS
	HANDLE hThread;
#else
	pthread_t thread;
#endif // LS_WINDOWS

	ls_thread_func_t func;
	void *up;

	unsigned long id;
};

struct ls_thread_self
{
	struct ls_handle_info hi;
	struct ls_thread th;
};


#endif // _LS_THREADDEF_H_
