#ifndef _LS_EVENT_PRIV_H_
#define _LS_EVENT_PRIV_H_

#include "ls_native.h"

struct ls_event
{
#if LS_WINDOWS
    HANDLE hEvent;
#else
    ls_lock_t lock;
    ls_cond_t cond;
    int signaled;
#endif // LS_WINDOWS
};

#endif // _LS_EVENT_PRIV_H_
