#ifndef _LS_THREAD_H_
#define _LS_THREAD_H_

#include "ls_defs.h"

typedef int(*ls_thread_func_t)(void *up);

ls_handle ls_thread_create(ls_thread_func_t func, void *up);

unsigned long ls_thread_id(ls_handle th);

unsigned long ls_thread_self(void);

void ls_yield(void);

#endif // _LS_THREAD_H_
