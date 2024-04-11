#ifndef _LS_WATCH_DARWIN_H_
#define _LS_WATCH_DARWIN_H_

#include <lysys/ls_watch.h>

typedef struct _ls_watch_darwin *ls_watch_darwin_t;

ls_watch_darwin_t ls_watch_darwin_alloc(const char *dir, int recursive);
void ls_watch_darwin_free(ls_watch_darwin_t w);
int ls_watch_darwin_wait(ls_watch_darwin_t w, unsigned long ms);
int ls_watch_darwin_get_result(ls_watch_darwin_t w, struct ls_watch_event *event);

#endif // _LS_WATCH_DARWIN_H_
