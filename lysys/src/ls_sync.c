#include "ls_native.h"

#include <lysys/ls_sync.h>

#include "ls_handle.h"

static struct ls_class LockClass = {
	.type = LS_LOCK,
#if LS_WINDOWS
	.cb = sizeof(CRITICAL_SECTION),
	.dtor = (ls_dtor_t)&DeleteCriticalSection,
#endif
	.wait = NULL
};

static struct ls_class ConditionClass = {
	.type = LS_COND,
#if LS_WINDOWS
	.cb = sizeof(CONDITION_VARIABLE),
	.dtor = NULL,
#endif
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
#endif
}

void ls_lock(ls_handle lock)
{
#if LS_WINDOWS
	EnterCriticalSection(lock);
#endif
}

int ls_trylock(ls_handle lock)
{
#if LS_WINDOWS
	return TryEnterCriticalSection(lock);
#endif
}

void ls_unlock(ls_handle lock)
{
#if LS_WINDOWS
	LeaveCriticalSection(lock);
#endif
}

ls_handle ls_cond_create(void)
{
#if LS_WINDOWS
	PCONDITION_VARIABLE pCV;

	pCV = ls_handle_create(&ConditionClass);
	if (!pCV) return NULL;
	InitializeConditionVariable(pCV);
	return pCV;
#endif
}

void ls_cond_wait(ls_handle cond, ls_handle lock)
{
#if LS_WINDOWS
	SleepConditionVariableCS(cond, lock, INFINITE);
#endif
}

int ls_cond_timedwait(ls_handle cond, ls_handle lock, unsigned long ms)
{
#if LS_WINDOWS
	return !SleepConditionVariableCS(cond, lock, ms);
#endif
}

void ls_cond_signal(ls_handle cond)
{
#if LS_WINDOWS
	WakeConditionVariable(cond);
#endif
}

void ls_cond_broadcast(ls_handle cond)
{
#if LS_WINDOWS
	WakeAllConditionVariable(cond);
#endif
}
