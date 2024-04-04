#ifndef _LS_FSEVENT_H_
#define _LS_FSEVENT_H_

#include "ls_defs.h"

#define LS_WATCH_ADD 1
#define LS_WATCH_REMOVE 2
#define LS_WATCH_MODIFY 3
#define LS_WATCH_RENAME 4

struct ls_watch_event
{
	int action;			//!< Action
	const char *target;	//!< Target path
	const char *source;	//!< Source path (if action is rename, otherwise NULL)
};

ls_handle ls_watch_dir(const char *dir, int recursive);

int ls_watch_poll(ls_handle watch);

int ls_watch_get_result(ls_handle watch, struct ls_watch_event *event);

#endif // _LS_FSEVENT_H_
