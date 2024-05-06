#include "ls_sync_util.h"

#include <lysys/ls_core.h>

int lock_init(ls_lock_t *lock)
{
#if LS_WINDOWS
    InitializeCriticalSection(lock);
    return 0;
#else
    return ls_set_errno(ls_errno_to_error(pthread_mutex_init(lock, NULL)));
#endif // LS_WINDOWS
}

void lock_destroy(ls_lock_t *lock)
{
#if LS_WINDOWS
    DeleteCriticalSection(lock);
#else
    if (pthread_mutex_destroy(lock) != 0)
        abort();
#endif // LS_WINDOWS
}

void lock_lock(ls_lock_t *lock)
{
#if LS_WINDOWS
    EnterCriticalSection(lock);
#else
    int rc;
    
    rc = pthread_mutex_lock(lock);
    if (rc == 0)
        return;
    
    errno = rc;
    perror("pthread_mutex_lock");
    abort();
#endif // LS_WINDOWS
}

int lock_trylock(ls_lock_t *lock)
{
#if LS_WINDOWS
    return TryEnterCriticalSection(lock) ? 0 : 1;
#else
    int rc = pthread_mutex_trylock(lock);
    if (rc == 0)
        return 0;
    
    if (rc == EBUSY)
        return 1;
    
    errno = rc;
    perror("pthread_mutex_trylock");
    abort();
#endif // LS_WINDOWS
}

void lock_unlock(ls_lock_t *lock)
{
#if LS_WINDOWS
	LeaveCriticalSection(lock);
#else
    int rc;
    
    rc = pthread_mutex_unlock(lock);
    if (rc == 0)
        return;
    
    errno = rc;
    perror("pthread_mutex_unlock");
    abort();
#endif // LS_WINDOWS
}

int cond_init(ls_cond_t *cond)
{
#if LS_WINDOWS
    InitializeConditionVariable(cond);
    return 0;
#else
    return ls_set_errno_errno(pthread_cond_init(cond, NULL));
#endif // LS_WINDOWS
}

void cond_destroy(ls_cond_t *cond)
{
#if LS_WINDOWS
#else
    int rc;
    
    rc = pthread_cond_destroy(cond);
    if (rc == 0)
        return;
    
    errno = rc;
    perror("pthread_cond_destroy");
    abort();
#endif // LS_WINDOWS
}

int cond_wait(ls_cond_t *RESTRICT cond, ls_lock_t *RESTRICT lock, unsigned long ms)
{
#if LS_WINDOWS
    if (!SleepConditionVariableCS(cond, lock, ms))
    {
        if (GetLastError() == ERROR_TIMEOUT)
			return 1;
		
		abort();
	}

    return 0;
#else
    int rc;
    struct timespec ts;
    
    if (ms == LS_INFINITE)
    {
        rc = pthread_cond_wait(cond, lock);
        if (rc == 0)
            return 0;
        
        errno = rc;
        perror("pthread_cond_wait");
        abort();
    }
    
    clock_gettime(CLOCK_REALTIME, &ts);
    
    ts.tv_sec += ms / 1000;
    ts.tv_nsec += (ms % 1000) * 1000000;
    
    rc = pthread_cond_timedwait(cond, lock, &ts);
    if (rc == 0)
        return 0;
    
    if (rc == ETIMEDOUT)
        return 1;

    errno = rc;
    perror("pthread_cond_timedwait");
    abort();
#endif // LS_WINDOWS
}

void cond_signal(ls_cond_t *cond)
{
#if LS_WINDOWS
    WakeConditionVariable(cond);
#else
    (void)pthread_cond_signal(cond);
#endif // LS_WINDOWS
}

void cond_broadcast(ls_cond_t *cond)
{
#if LS_WINDOWS
    WakeAllConditionVariable(cond);
#else
    (void)pthread_cond_broadcast(cond);
#endif // LS_WINDOWS
}
