#include "ls_native.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>

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

	if (!func)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	th = ls_handle_create(&ThreadClass, 0);
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

	if (!func)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	th = ls_handle_create(&ThreadClass, 0);
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

	if (th == LS_SELF)
		return ls_thread_id_self();

	if (ls_type_check(th, LS_THREAD))
		return -1;

	return t->id;
}

int ls_thread_set_priority(ls_handle th, int priority)
{
#if LS_WINDOWS
	struct ls_thread *t = th;
	int nPriority;
	HANDLE hThread;

	switch (priority)
	{
	default:
		return ls_set_errno(LS_INVALID_ARGUMENT);
	case LS_THREAD_PRIORITY_LOWEST:
		nPriority = THREAD_PRIORITY_LOWEST;
		break;
	case LS_THREAD_PRIORITY_BELOW_NORMAL:
		nPriority = THREAD_PRIORITY_BELOW_NORMAL;
		break;
	case LS_THREAD_PRIORITY_NORMAL:
		nPriority = THREAD_PRIORITY_NORMAL;
		break;
	case LS_THREAD_PRIORITY_ABOVE_NORMAL:
		nPriority = THREAD_PRIORITY_ABOVE_NORMAL;
		break;
	case LS_THREAD_PRIORITY_HIGHEST:
		nPriority = THREAD_PRIORITY_HIGHEST;
		break;
	}

	if (th == LS_SELF)
		hThread = GetCurrentThread();
	else
	{
		if (ls_type_check(th, LS_THREAD))
			return -1;
		hThread = t->hThread;
	}

	if (!SetThreadPriority(hThread, nPriority))
		return ls_set_errno_win32(GetLastError());

	return 0;
#else
	return ls_set_errno(LS_NOT_IMPLEMENTED);
#endif // LS_WINDOWS
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

	tls = ls_handle_create(&TlsClass, 0);
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

	if (ls_type_check(tlsh, LS_TLS))
		return -1;

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

	if (ls_type_check(tlsh, LS_TLS))
		return NULL;

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

struct ls_fiber
{
	ls_thread_func_t func;
	void *up;
	int exit_code;

#if LS_WINDOWS
	LPVOID lpFiber;
#else
    void *stack;
	ucontext_t ctx;
#endif // LS_WINDOWS
};

static LS_THREADLOCAL struct ls_fiber _main_fiber = { 0 };

#if LS_POSIX
static LS_THREADLOCAL struct ls_fiber *_current_fiber = NULL;
#endif // LS_POSIX

static void ls_fiber_dtor(struct ls_fiber *fiber)
{
#if LS_WINDOWS
	DeleteFiber(fiber->lpFiber);
#else
	if (fiber == &_main_fiber)
		abort(); // Cannot delete main fiber
    
    if (_current_fiber != &_main_fiber && fiber == _current_fiber)
        abort(); // Cannot delete self

    free(fiber->stack);
#endif // LS_WINDOWS
}

static const struct ls_class FiberClass = {
	.type = LS_FIBER,
	.cb = sizeof(struct ls_fiber),
	.dtor = (ls_dtor_t)&ls_fiber_dtor,
	.wait = NULL
};

#if LS_WINDOWS

static void CALLBACK ls_fiber_entry_thunk(void *up)
{
	struct ls_fiber *fiber = up;
	fiber->exit_code = fiber->func(fiber->up);
    ls_fiber_sched();
}

#else

static void *ls_create_pointer(int lo, int hi)
{
#if __SIZEOF_SIZE_T__ == 4
	return (void *)lo;
#else
	return (void *)(((uint64_t)hi << 32) | (uint64_t)lo);
#endif // __SIZEOF_SIZE_T__
}

static void ls_split_pointer(void *ptr, int *lo, int *hi)
{
#if __SIZEOF_SIZE_T__ == 4
	*lo = (int)ptr;
	*hi = 0;
#else
	*lo = (int)((uint64_t)ptr & 0xffffffff);
	*hi = (int)((uint64_t)ptr >> 32);
#endif // __SIZEOF_SIZE_T__
}

static void ls_fiber_entry_thunk(int lo, int hi)
{
	struct ls_fiber *fiber = ls_create_pointer(lo, hi);
	fiber->exit_code = fiber->func(fiber->up);
}

#endif // LS_WINDOWS

static inline struct ls_fiber *ls_resolve_fiber(ls_handle f)
{
#if LS_WINDOWS
	struct ls_fiber *fiber;

	if (ls_type_check(f, LS_FIBER))
		return NULL;

	if (f == LS_SELF)
	{
		fiber = GetCurrentFiber();
		if (!fiber)
		{
			ls_set_errno(LS_INVALID_HANDLE);
			return NULL;
		}

		return fiber;
	}

	if (f == LS_MAIN)
	{
		if (_main_fiber.lpFiber)
			return &_main_fiber;

		ls_set_errno(LS_INVALID_HANDLE);
		return NULL;
	}

	return f;
#else
	struct ls_fiber *fiber;

	if (ls_type_check(f, LS_FIBER))
		return NULL;
    
    if (!_current_fiber)
    {
        ls_set_errno(LS_INVALID_HANDLE);
        return NULL;
    }

	if (f == LS_SELF)
		return _current_fiber;
    
	if (f == LS_MAIN)
        return &_main_fiber;

	return f;
#endif // LS_WINDOWS
}

int ls_convert_to_fiber(void *up)
{
#if LS_WINDOWS
	if (_main_fiber.lpFiber)
		return 0;

	_main_fiber.lpFiber = ConvertThreadToFiber(NULL);
	if (!_main_fiber.lpFiber)
		return ls_set_errno_win32(GetLastError());

	_main_fiber.func = NULL;
	_main_fiber.up = up;

	return 0;
#else
	int rc;
	
	if (_current_fiber)
		return 0;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	rc = getcontext(&_main_fiber.ctx);
#pragma clang diagnostic pop

	if (rc == -1)
		return ls_set_errno_errno(errno);

	_main_fiber.func = NULL;
	_main_fiber.up = up;

	_current_fiber = &_main_fiber;

	return 0;
#endif // LS_WINDOWS
}

int ls_convert_to_thread(void)
{
#if LS_WINDOWS
	if (!_main_fiber.lpFiber)
		return 0;

	if (!ConvertFiberToThread())
		return ls_set_errno_win32(GetLastError());

	_main_fiber.lpFiber = NULL;
	_main_fiber.up = NULL;

	return 0;
#else
	if (!_current_fiber)
		return 0;

	if (_current_fiber != &_main_fiber)
		return ls_set_errno(LS_INVALID_STATE);

	memset(&_main_fiber.ctx, 0, sizeof(_main_fiber.ctx));
	_current_fiber = NULL;

	return 0;
#endif // LS_WINDOWS
}

ls_handle ls_fiber_create(ls_thread_func_t func, void *up)
{
#if LS_WINDOWS
	struct ls_fiber *f;

	if (!func)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	if (!_main_fiber.lpFiber)
	{
		// current thread is not a fiber
		ls_set_errno(LS_INVALID_STATE);
		return NULL;
	}

	f = ls_handle_create(&FiberClass, 0);
	if (!f)
		return NULL;

	f->lpFiber = CreateFiber(0, &ls_fiber_entry_thunk, f);
	if (!f->lpFiber)
	{
		ls_set_errno_win32(GetLastError());
		ls_handle_dealloc(f);
		return NULL;
	}

	f->func = func;
	f->up = up;

	return f;
#else
	struct ls_fiber *f;
	int rc;
	int lo, hi;

	if (!func)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	if (!_current_fiber)
	{
		// current thread is not a fiber
		ls_set_errno(LS_INVALID_STATE);
		return NULL;
	}

	f = ls_handle_create(&FiberClass, 0);
	if (!f)
		return NULL;
    
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    rc = getcontext(&f->ctx);
#pragma clang diagnostic pop
    
    f->stack = malloc(SIGSTKSZ);
    if (!f->stack)
    {
        ls_set_errno(LS_OUT_OF_MEMORY);
        ls_handle_dealloc(f);
        return NULL;
    }
    
    f->ctx.uc_stack.ss_sp = f->stack;
    f->ctx.uc_stack.ss_size = SIGSTKSZ;
    f->ctx.uc_link = 0;

	if (rc == -1)
	{
		ls_set_errno_errno(errno);
        free(f->stack);
		ls_handle_dealloc(f);
		return NULL;
	}

	// ucontext function takes int as arguments, so we need to split the pointer
	ls_split_pointer(f, &lo, &hi);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	makecontext(&f->ctx, (void(*)(void))&ls_fiber_entry_thunk, 2, lo, hi);
#pragma clang diagnostic pop
    
	f->func = func;
	f->up = up;

	return f;
#endif // LS_WINDOWS
}

void ls_fiber_switch(ls_handle fiber)
{
#if LS_WINDOWS
	struct ls_fiber *f;

	if (!_main_fiber.lpFiber)
		return; // Current thread is not a fiber

	f = ls_resolve_fiber(fiber);
	if (f)
		SwitchToFiber(f->lpFiber);
#else
	struct ls_fiber *f;
    ucontext_t *old_ctx;
    
	if (!_current_fiber)
		return; // Current thread is not a fiber

	f = ls_resolve_fiber(fiber);
	if (f == _current_fiber)
		return; // Already on this fiber

	if (f)
	{
        old_ctx = &_current_fiber->ctx;
		_current_fiber = f;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        swapcontext(old_ctx, &f->ctx);
#pragma clang diagnostic pop
	}
#endif // LS_WINDOWS
}

void ls_fiber_sched(void)
{
	ls_fiber_switch(LS_MAIN);
}

ls_handle ls_fiber_self(void)
{
#if LS_WINDOWS
	return GetCurrentFiber() ? LS_SELF : NULL;
#else
	return _main_fiber.ctx.uc_stack.ss_sp ? LS_SELF : NULL;
#endif // LS_WINDOWS
}

void *ls_fiber_get_data(ls_handle fiber)
{
	struct ls_fiber *f;
	f = ls_resolve_fiber(fiber);
	return f ? f->up : NULL;
}

LS_NORETURN void ls_fiber_exit(int code)
{
#if LS_WINDOWS
	struct ls_fiber *f;

	f = GetFiberData();

	if (!f)
		ExitThread(code); // Not a fiber

	f->exit_code = code;

	if (f == &_main_fiber)
		ExitThread(code); // Main fiber

	if (!_main_fiber.lpFiber)
		ExitThread(code); // No fiber to switch to

	SwitchToFiber(_main_fiber.lpFiber);
#else
	struct ls_fiber *f;

	f = ls_resolve_fiber(LS_SELF);

	if (!f)
		pthread_exit((void *)(intptr_t)code);

	f->exit_code = code;

	if (f == &_main_fiber)
		pthread_exit((void *)(intptr_t)code);

	if (!_current_fiber)
		pthread_exit((void *)(intptr_t)code);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	setcontext(&_main_fiber.ctx);
#pragma clang diagnostic pop

	abort(); // Someone switched to an exited fiber
#endif // LS_WINDOWS
}
