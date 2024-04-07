#include <lysys/ls_watch.h>

#include <lysys/ls_core.h>

#include <string.h>
#include <assert.h>

#include "ls_handle.h"
#include "ls_native.h"

#if LS_WINDOWS
#define NOTIF_BUFSIZE 1024
#endif // LS_WINDOWS

#define WATCH_ERROR -1 // thread error
#define WATCH_RUNNING 0 // thread running
#define WATCH_NOT_STARTED 1 // thread not started
#define WATCH_CLOSING 2 // explicit close by user

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

	CRITICAL_SECTION cs;
	CONDITION_VARIABLE cv;

	int recursive;

	int state; // thread state

	char source[MAX_PATH];
	char target[MAX_PATH];

	struct ls_watch_event_imp *front, *back;
#else
#endif // LS_WINDOWS
};

#if LS_WINDOWS

static DWORD WINAPI WatchThreadProc(LPVOID lpParam)
{
	struct ls_watch *w = *(struct ls_watch **)lpParam;
	struct ls_watch_event_imp *e, *next;
	BOOL b;
	DWORD dwBytes;
	HANDLE hDirectory;
	int recursive;

	union
	{
		FILE_NOTIFY_EXTENDED_INFORMATION fnei;
		BYTE buf[NOTIF_BUFSIZE];
	} u;

	PFILE_NOTIFY_EXTENDED_INFORMATION pNotify;

	EnterCriticalSection(&w->cs);

	assert(w->state == WATCH_NOT_STARTED);

	// save handle and flags

	hDirectory = w->hDirectory;
	recursive = w->recursive;

	// signal that we're running
	w->state = WATCH_RUNNING;
	WakeAllConditionVariable(&w->cv);

	do
	{
		// lock is held
		LeaveCriticalSection(&w->cs);

		ZeroMemory(&u.buf, NOTIF_BUFSIZE);

		// read changes
		b = ReadDirectoryChangesExW(
			hDirectory,
			u.buf,
			NOTIF_BUFSIZE,
			recursive,
			FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_SECURITY,
			&dwBytes,
			NULL,
			NULL,
			ReadDirectoryNotifyExtendedInformation);
		if (!b)
		{
			EnterCriticalSection(&w->cs);
			w->state = WATCH_ERROR;
			break;
		}

		EnterCriticalSection(&w->cs);
		if (w->state == WATCH_CLOSING)
			break;
		LeaveCriticalSection(&w->cs);

		// store event

		e = ls_malloc(sizeof(struct ls_watch_event_imp));
		if (!e)
		{
			EnterCriticalSection(&w->cs);
			w->state = WATCH_ERROR;
			break;
		}

		e->next = NULL;

		ZeroMemory(e->source, MAX_PATH);
		ZeroMemory(e->target, MAX_PATH);

		switch (u.fnei.Action)
		{
		case FILE_ACTION_ADDED:
			e->action = LS_WATCH_ADD;
			ls_wchar_to_utf8_buf(u.fnei.FileName, e->source, MAX_PATH);
			break;
		case FILE_ACTION_REMOVED:
			e->action = LS_WATCH_REMOVE;
			ls_wchar_to_utf8_buf(u.fnei.FileName, e->source, MAX_PATH);
			break;
		case FILE_ACTION_MODIFIED:
			e->action = LS_WATCH_MODIFY;
			ls_wchar_to_utf8_buf(u.fnei.FileName, e->source, MAX_PATH);
			break;
		case FILE_ACTION_RENAMED_OLD_NAME:
			if (u.fnei.NextEntryOffset == 0) // sanity check
			{
				// ignore
				ls_free(e), e = NULL;
				break;
			}

			pNotify = (PFILE_NOTIFY_EXTENDED_INFORMATION)(u.buf + u.fnei.NextEntryOffset);
			if (pNotify->Action != FILE_ACTION_RENAMED_NEW_NAME)
			{
				// ignore
				ls_free(e), e = NULL;
				break;
			}

			e->action = LS_WATCH_RENAME;
			ls_wchar_to_utf8_buf(u.fnei.FileName, e->source, MAX_PATH);
			ls_wchar_to_utf8_buf(pNotify->FileName, e->target, MAX_PATH);

			break;
		case FILE_ACTION_RENAMED_NEW_NAME: // always follows OLD_NAME
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
	} while (w->state == WATCH_RUNNING);

	// lock is held

	WakeAllConditionVariable(&w->cv); // prevent deadlock

	// wait until user closes the watch
	while (w->state == WATCH_RUNNING)
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

	CloseHandle(hDirectory);
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

	// signal thread to close
	EnterCriticalSection(&w->cs);
	w->state = WATCH_CLOSING;
	WakeAllConditionVariable(&w->cv);
	LeaveCriticalSection(&w->cs);

	// thread will clean up the rest
#else
#endif // LS_WINDOWS
}

static int LS_CLASS_FN ls_watch_wait(struct ls_watch **pw, unsigned long ms)
{
#if LS_WINDOWS
	struct ls_watch *w = *pw;
	DWORD dwRet;

	EnterCriticalSection(&w->cs);

	if (w->state != WATCH_RUNNING)
	{
		LeaveCriticalSection(&w->cs);
		return -1;
	}

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
	HANDLE hThread;
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
	w->state = WATCH_NOT_STARTED;

	InitializeCriticalSection(&w->cs);
	InitializeConditionVariable(&w->cv);

	rc = ls_utf8_to_wchar_buf(dir, szPath, MAX_PATH);
	if (!rc)
	{
		DeleteCriticalSection(&w->cs);
		ls_free(w), *h = NULL;

		ls_close(h);
		return NULL;
	}

	w->recursive = !!recursive;

	w->hDirectory = CreateFileW(szPath, FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (w->hDirectory == INVALID_HANDLE_VALUE)
	{
		DeleteCriticalSection(&w->cs);
		ls_free(w), *h = NULL;

		ls_close(h);
		return NULL;
	}

	hThread = CreateThread(NULL, 0, &WatchThreadProc, h, 0, NULL);
	if (!hThread)
	{
		CloseHandle(w->hDirectory);
		DeleteCriticalSection(&w->cs);
		ls_free(w), *h = NULL;

		ls_close(h);
		return NULL;
	}

	// wait for thread to initialize
	EnterCriticalSection(&w->cs);
	while (w->state == WATCH_NOT_STARTED)
		SleepConditionVariableCS(&w->cv, &w->cs, INFINITE);
	LeaveCriticalSection(&w->cs);

	CloseHandle(hThread); // detach

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
