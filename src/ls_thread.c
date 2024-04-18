#include "ls_native.h"

#include <signal.h>

#include <lysys/ls_thread.h>
#include <lysys/ls_core.h>

#include "ls_handle.h"

struct ls_thread
{
#if LS_WINDOWS
	HANDLE hThread;
#else
	pthread_t thread;
	int active;
#endif // LS_WINDOWS

	ls_thread_func_t func;
	void *up;
};

static void LS_CLASS_FN ls_thread_dtor(struct ls_thread *th)
{
#if LS_WINDOWS
	if (th->hThread)
		CloseHandle(th->hThread);
#else
	if (th->active)
		pthread_detach(th->thread);
#endif // LS_WINDOWS
}

static int LS_CLASS_FN ls_thread_wait(struct ls_thread *th)
{
#if LS_WINDOWS
	DWORD dwResult;

	if (!th->hThread)
		return 0;

	dwResult = WaitForSingleObject(th->hThread, INFINITE);
	if (dwResult == WAIT_OBJECT_0)
	{
		CloseHandle(th->hThread);
		th->hThread = NULL;
		return 0;
	}
	
	return -1;
#else
	int rc;

	if (!th->active)
		return 0;

	rc = pthread_join(th->thread, NULL);
	if (rc != 0) return -1;

	th->active = 0;
	return 0;
#endif // LS_WINDOWS
}

#if LS_WINDOWS
static DWORD CALLBACK ls_thread_startup(struct ls_thread *th)
{
	th->func(th->up);
	return 0;
}
#else

static void *ls_thread_startup(void *param)
{
	struct ls_thread *th = param;
	th->func(th->up);
	return NULL;
}

#endif // LS_WINDOWS

static const struct ls_class ThreadClass = {
	.type = LS_THREAD,
	.cb = sizeof(struct ls_thread),
	.dtor = (ls_dtor_t)&ls_thread_dtor,
	.wait = (ls_wait_t)&ls_thread_wait,
};

ls_handle ls_thread_create(ls_thread_func_t func, void *up)
{
#if LS_WINDOWS
	struct ls_thread *th;

	th = ls_handle_create(&ThreadClass);
	if (!th) return NULL;

	th->func = func;
	th->up = up;

	th->hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&ls_thread_startup, th, 0, NULL);
	if (!th->hThread)
	{
		ls_handle_dealloc(th);
		return NULL;
	}

	return th;
#else
	struct ls_thread *th;
	int rc;

	th = ls_handle_create(&ThreadClass);
	if (!th) return NULL;

	th->func = func;
	th->up = up;
	th->active = 1;

	rc = pthread_create(&th->thread, NULL, &ls_thread_startup, th);
	if (rc != 0)
	{
		ls_handle_dealloc(th);
		return NULL;
	}

	return th;
#endif // LS_WINDOWS
}

unsigned long ls_thread_id(ls_handle th)
{
#if LS_WINDOWS
	struct ls_thread *t = *(struct ls_thread **)th;
	return GetThreadId(t->hThread);
#else
	struct ls_thread *t = *(struct ls_thread **)th;
	return t->thread;
#endif // LS_WINDOWS
}

unsigned long ls_thread_self(void)
{
#if LS_WINDOWS
	return GetCurrentThreadId();
#else
	return pthread_self();
#endif // LS_WINDOWS
}

int ls_is_active_thread_id(unsigned long id)
{
#if LS_WINDOWS
	HANDLE hThread;
	hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, id);
	if (hThread)
	{
		CloseHandle(hThread);
		return 1;
	}

	return 0;
#else
	return pthread_kill(id, 0) == 0;
#endif // LS_WINDOWS
}

void ls_yield(void)
{
#if LS_WINDOWS
	SwitchToThread();
#else
	sched_yield();
#endif // LS_WINDOWS
}

struct ls_tls
{
#if LS_WINDOWS
	DWORD dwTlsIndex;
#else
	pthread_key_t key;
#endif // LS_WINDOWS
};

static void LS_CLASS_FN ls_tls_dtor(struct ls_tls *tls)
{
#if LS_WINDOWS
	TlsFree(tls->dwTlsIndex);
#else
	// TODO: Implement
#endif // LS_WINDOWS
}

static const struct ls_class TlsClass = {
	.type = LS_TLS,
	.cb = sizeof(struct ls_tls),
	.dtor = (ls_dtor_t)&ls_tls_dtor,
	.wait = NULL,
};

ls_handle ls_tls_create(void)
{
#if LS_WINDOWS
	struct ls_tls *tls;

	tls = ls_handle_create(&TlsClass);
	if (!tls)
		return NULL;

	tls->dwTlsIndex = TlsAlloc();
	if (tls->dwTlsIndex == TLS_OUT_OF_INDEXES)
	{
		ls_handle_dealloc(tls);
		return NULL;
	}

	return tls;
#else
	// TODO: Implement
	return NULL;
#endif // LS_WINDOWS
}

int ls_tls_set(ls_handle tlsh, void *value)
{
#if LS_WINDOWS
	struct ls_tls *tls = tlsh;
	return TlsSetValue(tls->dwTlsIndex, value) ? 0 : -1;
#else
	// TODO: Implement
	return -1;
#endif // LS_WINDOWS
}

void *ls_tls_get(ls_handle tlsh)
{
#if LS_WINDOWS
	struct ls_tls *tls = tlsh;
	return TlsGetValue(tls->dwTlsIndex);
#else
	// TODO: Implement
	return NULL;
#endif // LS_WINDOWS
}
