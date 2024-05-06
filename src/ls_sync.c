#include "ls_native.h"

#include <stdlib.h>
#include <stdio.h>

#include <lysys/ls_sync.h>
#include <lysys/ls_core.h>

#include "ls_handle.h"
#include "ls_sync_util.h"

static void ls_lock_dtor(void *param)
{
    lock_destroy(param);
}

static const struct ls_class LockClass = {
	.type = LS_LOCK,
	.cb = sizeof(ls_lock_t),
    .dtor = &ls_lock_dtor,
	.wait = NULL
};

static void ls_cond_dtor(void *param)
{
    cond_destroy(param);
}

static const struct ls_class ConditionClass = {
	.type = LS_COND,
    .cb = sizeof(ls_cond_t),
    .dtor = &ls_cond_dtor,
	.wait = NULL
};

ls_handle ls_lock_create(void)
{
    ls_lock_t *lock;
    int rc;
    
    lock = ls_handle_create(&LockClass);
    if (!lock)
        return NULL;
    
    rc = lock_init(lock);
    if (rc == -1)
    {
        ls_handle_dealloc(lock);
        return NULL;
    }
    
    return lock;
}

void ls_lock(ls_handle lock)
{
    lock_lock(lock);
}

int ls_trylock(ls_handle lock)
{
    return lock_trylock(lock);
}

void ls_unlock(ls_handle lock)
{
    lock_unlock(lock);
}

ls_handle ls_cond_create(void)
{
    ls_cond_t *cond;
    int rc;
    
    cond = ls_handle_create(&ConditionClass);
    if (!cond)
        return NULL;
    
    rc = cond_init(cond);
    if (rc == -1)
    {
        ls_handle_dealloc(cond);
        return NULL;
    }
    
    return cond;
}

void ls_cond_wait(ls_handle cond, ls_handle lock)
{
    cond_wait(cond, lock, LS_INFINITE);
}

int ls_cond_timedwait(ls_handle cond, ls_handle lock, unsigned long ms)
{
    return cond_wait(cond, lock, ms);
}

void ls_cond_signal(ls_handle cond)
{
    cond_signal(cond);
}

void ls_cond_broadcast(ls_handle cond)
{
    cond_broadcast(cond);
}
