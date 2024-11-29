#include "ls_native.h"

#include <stdlib.h>
#include <stdio.h>

#include <lysys/ls_sync.h>
#include <lysys/ls_core.h>

#include "ls_handle.h"
#include "ls_sync_util.h"

#if LS_DARWIN
#include <mach/semaphore.h>
#include <mach/clock.h>
#include <mach/mach_time.h>
#endif // LS_DARWIN

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

struct semaphore
{
#if LS_WINDOWS
#elif LS_DARWIN
    semaphore_t sema;
#else
#endif // LS_WINDOWS
};

static void ls_semaphore_dtor(struct semaphore *sema)
{
#if LS_WINDOWS
#elif LS_DARWIN
    semaphore_destroy(mach_task_self(), sema->sema);
#else
#endif // LS_WINDOWS
}

static int ls_semaphore_wait(struct semaphore *sema, unsigned long timeout)
{
#if LS_WINDOWS
    return ls_set_errno(LS_NOT_IMPLEMENTED);
#elif LS_DARWIN
    kern_return_t kr;
    mach_timespec_t deadline;
    uint64_t abstime;
    mach_timebase_info_data_t tbi;
    uint64_t nanos;
    
    if (timeout == LS_INFINITE)
    {
        kr = semaphore_wait(sema->sema);
        return ls_set_errno_kr(kr);
    }
    
    abstime = mach_absolute_time();
    mach_timebase_info(&tbi);
    
    nanos = abstime * tbi.numer / tbi.denom;
    deadline.tv_sec = (unsigned int)(nanos / 1000000000);
    deadline.tv_nsec = nanos % 1000000000;
   
    deadline.tv_sec += (unsigned int)(timeout / 1000);
    deadline.tv_nsec += (timeout % 1000) * 1000000000;

    kr = semaphore_timedwait(sema->sema, deadline);
    if (kr == KERN_OPERATION_TIMED_OUT)
    {
        ls_set_errno(LS_TIMEDOUT);
        return 1;
    }
    
    return ls_set_errno_kr(kr);
#else
    return ls_set_errno(LS_NOT_IMPLEMENTED);
#endif // LS_WINDOWS
}

static const struct ls_class SemaphoreClass = {
    .type = LS_SEMAPHORE,
    .cb = sizeof(struct semaphore),
    .dtor = (ls_dtor_t)&ls_semaphore_dtor,
    .wait = (ls_wait_t)&ls_semaphore_wait
};

ls_handle ls_lock_create(void)
{
    ls_lock_t *lock;
    int rc;
    
    lock = ls_handle_create(&LockClass, 0);
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
    
    cond = ls_handle_create(&ConditionClass, 0);
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

ls_handle ls_semaphore_create(int count)
{
#if LS_WINDOWS
    ls_set_errno(LS_NOT_IMPLEMENTED);
    return NULL;
#elif LS_DARWIN
    struct semaphore *sema;
    kern_return_t kr;
    
    sema = ls_handle_create(&SemaphoreClass, 0);
    if (!sema)
        return NULL;
    
    kr = semaphore_create(mach_task_self(), &sema->sema, SYNC_POLICY_FIFO, 0);
    if (kr != KERN_SUCCESS)
    {
        ls_handle_dealloc(sema);
        ls_set_errno_kr(kr);
        return NULL;
    }
    
    return sema;
#else
    ls_set_errno(LS_NOT_IMPLEMENTED);
    return NULL;
#endif // LS_WNIDOWS
}

int ls_semaphore_signal(ls_handle sema)
{
#if LS_WINDOWS
    return ls_set_errno(LS_NOT_IMPLEMENTED);
#elif LS_DARWIN
    struct semaphore *semaphore = sema;
    kern_return_t kr;
    
    if (ls_type_check(semaphore, LS_SEMAPHORE) != 0)
        return -1;
    return ls_set_errno_kr(semaphore_signal(semaphore->sema));
#else
    return ls_set_errno(LS_NOT_IMPLEMENTED);
#endif // LS_WINDOWS
}
