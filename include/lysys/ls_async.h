#ifndef _LS_ASYNC_H_
#define _LS_ASYNC_H_

#include "ls_defs.h"

#define LS_TASK_STATUS_COMPLETE 0
#define LS_TASK_STATUS_IN_PROGRESS 1
#define LS_TASK_STATUS_CANCELED 2
#define LS_TASK_STATUS_ERROR 3

typedef int(*ls_task_completion_fn_t)(uintptr_t param, void **result);

int ls_async_init(void);

ls_atom ls_dispatch(ls_task_completion_fn_t fn, uintptr_t param);

int ls_get_async_status(ls_atom task_atom, void **result);

int ls_await(ls_atom task_atom, void **result);
int ls_ignore(ls_atom task_atom);
int ls_cancel(ls_atom task_atom);

#endif // _LS_ASYNC_H_
