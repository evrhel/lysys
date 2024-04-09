#include "ls_native.h"

#include <lysys/ls_sync.h>

#include "ls_handle.h"

static const struct ls_class LockClass = {
	.type = LS_LOCK,
#if LS_WINDOWS
	.cb = sizeof(CRITICAL_SECTION),
	.dtor = (ls_dtor_t)&DeleteCriticalSection,
#else
    .cb = sizeof(pthread_mutex_t),
    .dtor = (ls_dtor_t)&pthread_mutex_destroy,
#endif // LS_WINDOWS
	.wait = NULL
};

static const struct ls_class ConditionClass = {
	.type = LS_COND,
#if LS_WINDOWS
	.cb = sizeof(CONDITION_VARIABLE),
	.dtor = NULL,
#else
    .cb = sizeof(pthread_cond_t),
    .dtor = (ls_dtor_t)&pthread_cond_destroy,
#endif // LS_WINDOWS
	.wait = NULL
};

ls_handle ls_lock_create(void)
{
#if LS_WINDOWS
	LPCRITICAL_SECTION lpCS;
	lpCS = ls_handle_create(&LockClass);
	if (!lpCS) return NULL;
	InitializeCriticalSection(lpCS);
	return lpCS;
#else
    pthread_mutex_t *mutex;
    int rc;
    
    mutex = ls_handle_create(&LockClass);
    if (!mutex) return NULL;
    rc = pthread_mutex_init(mutex, NULL);
    if (rc != 0)
    {
        ls_handle_dealloc(mutex);
        return NULL;
    }
    
    return mutex;
#endif // LS_WINDOWS
}

void ls_lock(ls_handle lock)
{
#if LS_WINDOWS
    EnterCriticalSection(lock);
#else
    pthread_mutex_lock(lock);
#endif // LS_WINDOWS
}

int ls_trylock(ls_handle lock)
{
#if LS_WINDOWS
	return TryEnterCriticalSection(lock);
#else
    return !!pthread_mutex_trylock(lock);
#endif // LS_WINDOWS
}

void ls_unlock(ls_handle lock)
{
#if LS_WINDOWS
	LeaveCriticalSection(lock);
#else
    pthread_mutex_unlock(lock);
#endif // LS_WINDOWS
}

ls_handle ls_cond_create(void)
{
#if LS_WINDOWS
	PCONDITION_VARIABLE pCV;

	pCV = ls_handle_create(&ConditionClass);
	if (!pCV) return NULL;
	InitializeConditionVariable(pCV);
	return pCV;
#else
    pthread_cond_t *cond;
    int rc;
    
    cond = ls_handle_create(&ConditionClass);
    if (!cond) return NULL;
    
    rc = pthread_cond_init(cond, NULL);
    if (rc != 0)
    {
        ls_handle_dealloc(cond);
        return NULL;
    }
    
    return cond;
#endif // LS_WINDOWS
}

void ls_cond_wait(ls_handle cond, ls_handle lock)
{
#if LS_WINDOWS
	SleepConditionVariableCS(cond, lock, INFINITE);
#else
    pthread_cond_wait(cond, lock);
#endif // LS_WINDOWS
}

int ls_cond_timedwait(ls_handle cond, ls_handle lock, unsigned long ms)
{
#if LS_WINDOWS
	return !SleepConditionVariableCS(cond, lock, ms);
#else
    struct timespec ts;
    struct timeval tv;
    int rc;
    
    gettimeofday(&tv, NULL);
    
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ts.tv_sec - ms) * 1000000;
    
    ts.tv_sec += tv.tv_sec;
    ts.tv_nsec += tv.tv_usec * 1000;
    
    return !!pthread_cond_timedwait(cond, lock, &ts);
#endif // LS_WINDOWS
}

void ls_cond_signal(ls_handle cond)
{
#if LS_WINDOWS
	WakeConditionVariable(cond);
#else
    pthread_cond_signal(cond);
#endif // LS_WINDOWS
}

void ls_cond_broadcast(ls_handle cond)
{
#if LS_WINDOWS
	WakeAllConditionVariable(cond);
#else
    pthread_cond_broadcast(cond);
#endif // LS_WINDOWS
}
