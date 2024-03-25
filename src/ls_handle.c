#include "ls_handle.h"

#include <stdlib.h>
#include <memory.h>

#include <lysys/ls_core.h>

ls_handle ls_handle_create(struct ls_class *clazz)
{
	struct ls_handle_info *hi;
	
	hi = ls_calloc(1, sizeof(struct ls_handle_info) + clazz->cb);
	if (hi == NULL) return NULL;
	hi->clazz = clazz;
	return hi->data;
}

int ls_wait(ls_handle h)
{
	return ls_timedwait(h, LS_INFINITE);
}

int ls_timedwait(ls_handle h, unsigned long ms)
{
	struct ls_handle_info *hi;
	
	if (!h || h == (ls_handle)-1) return -1;
	
	hi = ls_get_handle_info(h);
	if (hi->clazz->wait)
		return hi->clazz->wait(hi->data, ms);
	return -1;
}

void ls_close(ls_handle h)
{
	struct ls_handle_info *hi;

	if (!h || h == (ls_handle)-1) return;

	hi = ls_get_handle_info(h);

	if (hi->clazz->dtor)
		hi->clazz->dtor(hi->data);
	ls_free(hi);
}
