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
	OVERLAPPED ov;
	HANDLE hThread; // thread that created this watch

	int recursive;

	union
	{
		FILE_NOTIFY_EXTENDED_INFORMATION fnei;
		BYTE buf[NOTIF_BUFSIZE];
	} u;

	char source[MAX_PATH];
	char target[MAX_PATH];

	struct ls_watch_event_imp *front, *back;
#else
#endif // LS_WINDOWS
};

#if LS_WINDOWS

static int dispatch_next_read(struct ls_watch *w)
{
	BOOL b;

	b = ReadDirectoryChangesExW(
		w->hDirectory,
		w->u.buf,
		NOTIF_BUFSIZE,
		w->recursive,
		FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_SECURITY,
		NULL,
		&w->ov,
		NULL,
		ReadDirectoryNotifyExtendedInformation);
	
	return b ? 0 : -1;
}

static int read_directory_changes(struct ls_watch *w)
{
	DWORD dwRet;
	DWORD dwBytes;

	dwRet = GetOverlappedResult(w->hDirectory, &w->ov, &dwBytes, FALSE);
	if (!dwRet)
	{
		if (ERROR_IO_INCOMPLETE == GetLastError())
			return 1;

		return -1;
	}

	return 0;
}

#endif // LS_WINDOWS

static void LS_CLASS_FN ls_watch_dtor(struct ls_watch *w)
{
#if LS_WINDOWS
	if (w->hDirectory && w->hDirectory != INVALID_HANDLE_VALUE)
		CloseHandle(w->hDirectory);
#else
#endif // LS_WINDOWS
}

static int LS_CLASS_FN ls_watch_wait(struct ls_watch *w, unsigned long ms)
{
#if LS_WINDOWS
	DWORD dwRet;

	dwRet = WaitForSingleObject(w->ov.hEvent, ms);
	if (dwRet == WAIT_OBJECT_0)
		return 0;

	if (dwRet == ERROR_TIMEOUT)
		return 1;
	
	return -1;
#else
	return -1;
#endif // LS_WINDOWS
}

static const struct ls_class WatchClass = {
	.type = LS_WATCH,
	.cb = sizeof(struct ls_watch),
	.dtor = (ls_dtor_t)&ls_watch_dtor,
	.wait = (ls_wait_t)&ls_watch_wait
};

ls_handle ls_watch_dir(const char *dir, int recursive)
{
#if LS_WINDOWS
	struct ls_watch *w;
	WCHAR szPath[MAX_PATH];
	int rc;

	w = ls_handle_create(&WatchClass);
	if (!w) return NULL;

	rc = ls_utf8_to_wchar_buf(dir, szPath, MAX_PATH);
	if (!rc)
	{
		ls_close(w);
		return NULL;
	}

	w->recursive = !!recursive;

	w->hDirectory = CreateFileW(szPath, FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (w->hDirectory == INVALID_HANDLE_VALUE)
	{
		ls_close(w);
		return NULL;
	}

	rc = dispatch_next_read(w);
	if (rc == -1)
	{
		ls_close(w);
		return NULL;
	}

	return w;
#else
	return NULL;
#endif // LS_WINDOWS
}

int ls_watch_get_result(ls_handle watch, struct ls_watch_event *event)
{
#if LS_WINDOWS
	struct ls_watch *w = watch;
	struct ls_watch_event_imp *e;
	int action;

	if (!w->front)
		return 1;

	e = w->front;
	w->front = w->front->next;
	if (!w->front)
		w->back = NULL;

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
