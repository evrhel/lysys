#ifndef _LS_EVENT_H_
#define _LS_EVENT_H_

#include "ls_defs.h"

ls_handle ls_event_create(void);

int ls_event_signaled(ls_handle evt);

int ls_event_set(ls_handle evt);

int ls_event_reset(ls_handle evt);

#endif
