#include <lysys/ls_watch.h>

#include <lysys/ls_core.h>

#include <string.h>
#include <assert.h>

#include "ls_handle.h"
#include "ls_native.h"

#if LS_WINDOWS

#define NOTIF_BUFSIZE 1024

#endif // LS_WINDOWS

struct ls_watch_event_imp
{
	int action;
#if LS_WINDOWS
	char source[MAX_PATH];
	char target[MAX_PATH];
#else

#endif // LS_WINDOWS

	struct ls_watch_event_imp *next;
};

struct ls_watch
{
#if LS_WINDOWS
	HANDLE hDirectory;

	HANDLE hThread;

	CRITICAL_SECTION cs;
	CONDITION_VARIABLE cv;

	int was_error;
	int recursive;
	int open;

	char source[MAX_PATH];
	char target[MAX_PATH];

	struct ls_watch_event_imp *front, *back;

	union
	{
		FILE_NOTIFY_EXTENDED_INFORMATION fnei;
		BYTE buf[NOTIF_BUFSIZE];
	} u;
#else
#endif // LS_WINDOWS
};

#if LS_WINDOWS

static DWORD WINAPI WatchThreadProc(LPVOID lpParam)
{
	struct ls_watch *w = *(struct ls_watch **)lpParam;
	struct ls_watch_event_imp *e, *next;
	BOOL b;
	
	EnterCriticalSection(&w->cs);
	w->open = 1;
	WakeAllConditionVariable(&w->cv);

	do
	{
		// lock is held
		LeaveCriticalSection(&w->cs);

		ZeroMemory(&w->u.buf, NOTIF_BUFSIZE);

		// read changes
		b = ReadDirectoryChangesExW(
			w->hDirectory,
			w->u.buf,
			NOTIF_BUFSIZE,
			w->recursive,
			FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_SECURITY,
			NULL,
			NULL,
			NULL,
			ReadDirectoryNotifyExtendedInformation);
		if (!b)
		{
			EnterCriticalSection(&w->cs);
			w->was_error = 1;
			break;
		}

		EnterCriticalSection(&w->cs);
		if (!w->open)
			break;
		LeaveCriticalSection(&w->cs);

		// store event

		e = ls_malloc(sizeof(struct ls_watch_event_imp));
		if (!e)
		{
			EnterCriticalSection(&w->cs);
			w->was_error = 1;
			break;
		}

		ZeroMemory(e->target, MAX_PATH);
		ZeroMemory(e->source, MAX_PATH);

		switch (w->u.fnei.Action)
		{
		case FILE_ACTION_ADDED:
			e->action = LS_WATCH_ADD;
			ls_wchar_to_utf8_buf(w->u.fnei.FileName, e->target, MAX_PATH);
			break;
		case FILE_ACTION_REMOVED:
			e->action = LS_WATCH_REMOVE;
			ls_wchar_to_utf8_buf(w->u.fnei.FileName, e->target, MAX_PATH);
			break;
		case FILE_ACTION_MODIFIED:
			e->action = LS_WATCH_MODIFY;
			ls_wchar_to_utf8_buf(w->u.fnei.FileName, e->target, MAX_PATH);
			break;
		case FILE_ACTION_RENAMED_OLD_NAME:
			// TODO: implement
		case FILE_ACTION_RENAMED_NEW_NAME:
			// TODO: implement
		default:
			// ignore
			ls_free(e), e = NULL;
			break;
		}

		if (e) // add to queue
		{
			EnterCriticalSection(&w->cs);

			if (w->back)
				w->back->next = e;
			else
				w->front = e;

			w->back = e;

			WakeAllConditionVariable(&w->cv);
		}
		else
			EnterCriticalSection(&w->cs);

		// lock is held
	} while (w->open);

	// lock is held

	// wait until user closes the watch
	while (w->open)
		SleepConditionVariableCS(&w->cv, &w->cs, INFINITE);
	LeaveCriticalSection(&w->cs);

	// free all outstanding events
	e = w->front;
	while (e)
	{
		next = e->next;
		ls_free(e);
		e = next;
	}

	w->front = NULL, w->back = NULL;

	CloseHandle(w->hDirectory);
	DeleteCriticalSection(&w->cs);	

	ls_free(w);

	return 0;
}

#endif // LS_WINDOWS

static void LS_CLASS_FN ls_watch_dtor(struct ls_watch **pw)
{
#if LS_WINDOWS
	struct ls_watch *w = *pw;

	if (!w) return;

	if (w->open)
	{
		assert(w->hThread);

		EnterCriticalSection(&w->cs);
		w->open = 0;
		WakeAllConditionVariable(&w->cv);
		LeaveCriticalSection(&w->cs);

		CloseHandle(w->hThread); // detach thread, don't wait

		// thread will clean up the rest
	}
	else
	{
		assert(!w->hThread);

		// our responsibility to clean up

		if (w->hDirectory && w->hDirectory != INVALID_HANDLE_VALUE)
			CloseHandle(w->hDirectory);

		DeleteCriticalSection(&w->cs);

		ls_free(w);
	}
#else
#endif // LS_WINDOWS
}

static int LS_CLASS_FN ls_watch_wait(struct ls_watch **pw, unsigned long ms)
{
#if LS_WINDOWS
	struct ls_watch *w = *pw;
	DWORD dwRet;

	EnterCriticalSection(&w->cs);
	if (!w->front)
	{
		dwRet = SleepConditionVariableCS(&w->cv, &w->cs, ms);
		if (!dwRet)
		{
			// lock not held

			if (ERROR_TIMEOUT == GetLastError())
				return 1;

			return -1;
		}
	}
	LeaveCriticalSection(&w->cs);

	return 0;
#else
	return -1;
#endif // LS_WINDOWS
}

static const struct ls_class WatchClass = {
	.type = LS_WATCH,
	.cb = sizeof(struct ls_watch *),
	.dtor = (ls_dtor_t)&ls_watch_dtor,
	.wait = (ls_wait_t)&ls_watch_wait
};

ls_handle ls_watch_dir(const char *dir, int recursive)
{
#if LS_WINDOWS
	struct ls_watch **h;
	struct ls_watch *w;
	WCHAR szPath[MAX_PATH];
	int rc;

	h = ls_handle_create(&WatchClass);
	if (!h) return NULL;

	*h = ls_calloc(1, sizeof(struct ls_watch));
	if (!*h)
	{
		ls_close(h);
		return NULL;
	}

	w = *h;

	InitializeCriticalSection(&w->cs);
	InitializeConditionVariable(&w->cv);

	rc = ls_utf8_to_wchar_buf(dir, szPath, MAX_PATH);
	if (!rc)
	{
		ls_close(h);
		return NULL;
	}

	w->recursive = !!recursive;

	w->hDirectory = CreateFileW(szPath, FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (w->hDirectory == INVALID_HANDLE_VALUE)
	{
		ls_close(h);
		return NULL;
	}


	w->hThread = CreateThread(NULL, 0, &WatchThreadProc, h, 0, NULL);
	if (!w->hThread)
	{
		ls_close(h);
		return NULL;
	}

	// wait for thread to initialize
	EnterCriticalSection(&w->cs);
	while (!w->open)
		SleepConditionVariableCS(&w->cv, &w->cs, INFINITE);
	LeaveCriticalSection(&w->cs);

	return h;
#else
	return NULL;
#endif // LS_WINDOWS
}

int ls_watch_get_result(ls_handle watch, struct ls_watch_event *event)
{
#if LS_WINDOWS
	struct ls_watch *w = *(struct ls_watch **)watch;
	struct ls_watch_event_imp *e;
	int action;

	EnterCriticalSection(&w->cs);

	if (!w->front)
	{
		LeaveCriticalSection(&w->cs);
		return 1;
	}

	e = w->front;
	w->front = w->front->next;
	if (!w->front)
		w->back = NULL;

	LeaveCriticalSection(&w->cs);

	action = e->action;
	memcpy(w->source, e->source, MAX_PATH);
	memcpy(w->target, e->target, MAX_PATH);

	ls_free(e);

	event->action = action;
	event->source = w->source[0] ? w->source : NULL;
	event->target = w->target[0] ? w->target : NULL;

	return 0;
#else
	return -1;
#endif // LS_WINDOWS
}
