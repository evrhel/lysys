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
#endif // LS_WINDOWS

	ls_thread_func_t func;
	void *up;

	unsigned long id;
};

struct ls_thread_self
{
	struct ls_handle_info hi;
	struct ls_thread th;
};

static void ls_thread_dtor(struct ls_thread *th)
{
#if LS_WINDOWS
	CloseHandle(th->hThread);
#else
	pthread_detach(th->thread);
#endif // LS_WINDOWS
}

static int ls_thread_wait(struct ls_thread *th, unsigned long ms)
{
#if LS_WINDOWS
	DWORD dwResult;

	if (th->id == GetCurrentThreadId())
		return ls_set_errno(LS_NOT_WAITABLE); // Myself

	dwResult = WaitForSingleObject(th->hThread, ms);
	if (dwResult == WAIT_OBJECT_0)
	{
		th->hThread = NULL;
		return 0;
	}

	if (dwResult == WAIT_TIMEOUT)
		return 1;
	
	return ls_set_errno_win32(GetLastError());
#else
	int rc;
    uint64_t id;
    
#if LS_DARWIN
    pthread_threadid_np(NULL, &id);
#else
    id = pthread_self();
#endif // LS_DARWIN

	if (th->id == id)
		return ls_set_errno(LS_NOT_WAITABLE); // Myself

	rc = pthread_join(th->thread, NULL);
	if (rc != 0)
	{
		if (rc == ESRCH)
			return 0; // Already waited

		return ls_set_errno(LS_INVALID_STATE);
	}

	return 0;
#endif // LS_WINDOWS
}

#if LS_WINDOWS
#define ENTRY_PROC DWORD CALLBACK
#define EXIT_OK 0
#else
#define ENTRY_PROC void *
#define EXIT_OK NULL
#endif // LS_WINDOWS

static ENTRY_PROC ls_thread_startup(struct ls_thread *th)
{
	th->func(th->up);
	return EXIT_OK;
}

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
	if (!th)
		return NULL;

	th->func = func;
	th->up = up;

	th->hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&ls_thread_startup, th, 0, &th->id);
	if (!th->hThread)
	{
		ls_set_errno_win32(GetLastError());
		ls_handle_dealloc(th);
		return NULL;
	}

	return th;
#else
	struct ls_thread *th;
	int rc;
#if LS_DARWIN
	uint64_t id;
#endif // LS_DARWIN

	th = ls_handle_create(&ThreadClass);
	if (!th)
		return NULL;

	th->func = func;
	th->up = up;

	rc = pthread_create(&th->thread, NULL, (void *(*)(void*))&ls_thread_startup, th);
	if (rc != 0)
	{
		ls_handle_dealloc(th);
		return NULL;
	}

#if LS_DARWIN
	(void)pthread_threadid_np(th->thread, &id);
	th->id = id;
#else
	th->id = th->thread;
#endif // LS_DARWIN

	return th;
#endif // LS_WINDOWS
}

unsigned long ls_thread_id(ls_handle th)
{
	struct ls_thread *t = th;

	if (!th)
		return ls_set_errno(LS_INVALID_HANDLE);

	if (th == LS_SELF)
		return ls_thread_id_self();

	return t->id;
}

unsigned long ls_thread_id_self(void)
{
#if LS_WINDOWS
	return GetCurrentThreadId();
#elif LS_DARWIN
	uint64_t id;
	(void)pthread_threadid_np(NULL, &id);
	return id;
#else
	return pthread_self();
#endif // LS_WINDOWS
}

ls_handle ls_thread_self(void) { return LS_SELF; }

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
#elif LS_DARWIN
	return ls_set_errno(LS_NOT_IMPLEMENTED); // TODO: Implement
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

static void ls_tls_dtor(struct ls_tls *tls)
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
		ls_set_errno_win32(GetLastError());
		ls_handle_dealloc(tls);
		ls_set_errno(LS_OUT_OF_MEMORY);
		return NULL;
	}

	return tls;
#else
	// TODO: Implement
	ls_set_errno(LS_NOT_IMPLEMENTED);
	return NULL;
#endif // LS_WINDOWS
}

int ls_tls_set(ls_handle tlsh, void *value)
{
#if LS_WINDOWS
	BOOL b;
	struct ls_tls *tls = tlsh;

	if (!tlsh)
		return ls_set_errno(LS_INVALID_HANDLE);

	b = TlsSetValue(tls->dwTlsIndex, value);
	if (!b)
		return ls_set_errno_win32(GetLastError());

	return 0;
#else
	// TODO: Implement
	ls_set_errno(LS_NOT_IMPLEMENTED);
	return -1;
#endif // LS_WINDOWS
}

void *ls_tls_get(ls_handle tlsh)
{
#if LS_WINDOWS
	LPVOID lpValue;
	struct ls_tls *tls = tlsh;
	DWORD dwError;

	lpValue = TlsGetValue(tls->dwTlsIndex);
	if (!lpValue)
	{
		dwError = GetLastError();
		if (dwError != ERROR_SUCCESS)
			ls_set_errno_win32(dwError);
	}
	return lpValue;
#else
	// TODO: Implement
	ls_set_errno(LS_NOT_IMPLEMENTED);
	return NULL;
#endif // LS_WINDOWS
}

struct ls_apc_info
{
	ls_apc_routine routine;
	void *arg;
};

#if LS_WINDOWS

static void CALLBACK ls_apc_func(ULONG_PTR Parameter)
{
	struct ls_apc_info *info = (struct ls_apc_info *)Parameter;
	info->routine(info->arg);
}

#endif // LS_WINDOWS

int ls_queue_apc(ls_apc_routine routine, unsigned long thread_id, void *arg)
{
#if LS_WINDOWS
	struct ls_apc_info *info;
	HANDLE hThread;
	DWORD dwRet;

	if (!routine)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	if (thread_id != 0)
		hThread = OpenThread(THREAD_SET_CONTEXT, FALSE, thread_id);
	else
		hThread = GetCurrentThread();

	if (!hThread)
		return ls_set_errno_win32(GetLastError());

	info = ls_malloc(sizeof(struct ls_apc_info));
	if (!info)
	{
		CloseHandle(hThread);
		return -1;
	}

	info->routine = routine;
	info->arg = arg;

	dwRet = QueueUserAPC(&ls_apc_func, hThread, (ULONG_PTR)info);

	CloseHandle(hThread);

	if (!dwRet)
	{
		dwRet = GetLastError();
		ls_free(info);
		return ls_set_errno_win32(dwRet);
	}

	return 0;
#else
	return ls_set_errno(LS_NOT_IMPLEMENTED);
#endif // LS_WINDOWS
}

void ls_set_alertable(void)
{
#if LS_WINDOWS
	SleepEx(0, TRUE);
#else
#endif // LS_WINDOWS
}
