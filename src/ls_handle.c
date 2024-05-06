#include "ls_handle.h"

#include <stdlib.h>
#include <memory.h>

#include <lysys/ls_core.h>

#include "ls_native.h"

ls_handle ls_handle_create(const struct ls_class *clazz)
{
	struct ls_handle_info *hi;
	
	hi = ls_calloc(1, sizeof(struct ls_handle_info) + clazz->cb);
	if (hi == NULL)
		return NULL;

	hi->clazz = clazz;
	hi->flags = 0;
	hi->refcount = 1;

	return LS_HANDLE_DATA(hi);
}

void ls_handle_dealloc(ls_handle h)
{
    struct ls_handle_info *hi;
    
    if (LS_IS_PSUEDO_HANDLE(h))
		return;
    
    hi = LS_HANDLE_INFO(h);
	if (!(hi->flags & LS_HANDLE_FLAG_STATIC))
		ls_free(hi);
}

int ls_wait(ls_handle h)
{
	return ls_timedwait(h, LS_INFINITE);
}

int ls_timedwait(ls_handle h, unsigned long ms)
{
	struct ls_handle_info *hi;
	
	if (LS_IS_PSUEDO_HANDLE(h))
		return ls_set_errno(LS_INVALID_HANDLE);
	
	hi = LS_HANDLE_INFO(h);
	if (hi->clazz->wait)
		return hi->clazz->wait(h, ms);

	return ls_set_errno(LS_NOT_WAITABLE);
}

void ls_close(ls_handle h)
{
	struct ls_handle_info *hi;

	if (LS_IS_PSUEDO_HANDLE(h))
		return;

	hi = LS_HANDLE_INFO(h);

	if (!(hi->flags & LS_HANDLE_FLAG_STATIC))
	{
		if (hi->clazz->dtor)
			hi->clazz->dtor(h);
	}
}

