#include <lysys/ls_watch.h>

#include <lysys/ls_core.h>

#include "ls_handle.h"
#include "ls_native.h"

#if LS_WINDOWS

#define NOTIF_BUFSIZE 1024

#define NOTIF_ALL (FILE_NOTIFY_CHANGE_FILE_NAME | \
	FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | \
	FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | \
	FILE_NOTIFY_CHANGE_LAST_ACCESS | FILE_NOTIFY_CHANGE_CREATION |\
	FILE_NOTIFY_CHANGE_SECURITY)

#endif // LS_WINDOWS

struct ls_watch
{
#if LS_WINDOWS
	HANDLE hDirectory;

	HANDLE hThread;
	HANDLE hEvent;

	int was_error;
	int recursive;

	// result
	int action;
	char szSource[MAX_PATH];
	char szTarget[MAX_PATH];
	
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
	struct ls_watch *w = lpParam;
}

#endif // LS_WINDOWS

static void LS_CLASS_FN ls_watch_dtor(struct ls_watch *w)
{
#if LS_WINDOWS
	if (w->hEvent)
		CloseHandle(w->hEvent);

	if (w->hThread)
		CloseHandle(w->hThread);

	if (w->hDirectory && w->hDirectory != INVALID_HANDLE_VALUE)
		CloseHandle(w->hDirectory);
#else
#endif // LS_WINDOWS
}

static int LS_CLASS_FN ls_watch_wait(struct ls_watch *w, unsigned long ms)
{
#if LS_WINDOWS
	DWORD dwRet;

	dwRet = WaitForSingleObject(w->hEvent, ms);
	if (dwRet == WAIT_OBJECT_0)
		return 0;

	if (dwRet == WAIT_TIMEOUT)
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
	BOOL b;

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
		NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if (w->hDirectory == INVALID_HANDLE_VALUE)
	{
		ls_close(w);
		return NULL;
	}

	w->hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
	if (!w->hEvent)
	{
		ls_close(w);
		return NULL;
	}

	b = ReadDirectoryChangesExW(
		w->hDirectory,
		w->u.buf,
		NOTIF_BUFSIZE,
		w->recursive,
		NOTIF_ALL,
		NULL,
		NULL,
		NULL,
		ReadDirectoryNotifyExtendedInformation);
	if (!b)
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
	BOOL b;
	DWORD dwBytes;
	DWORD dwErr;

	b = GetOverlappedResult(w->hDirectory, &w->ol, &dwBytes, FALSE);
	if (!b)
	{
		dwErr = GetLastError();
		if (dwErr == ERROR_IO_INCOMPLETE)
			return 1;
		return -1;
	}

	ResetEvent(w->ol.hEvent);

	switch (w->u.fnei.Action)
	{
	case FILE_ACTION_ADDED:
		break;
	case FILE_ACTION_REMOVED:
		break;
	case FILE_ACTION_MODIFIED:
		break;
	case FILE_ACTION_RENAMED_OLD_NAME:
		break;
	case FILE_ACTION_RENAMED_NEW_NAME:
		break;
	default:
		break;
	}

	b = ReadDirectoryChangesExW(
		w->hDirectory,
		w->u.buf,
		NOTIF_BUFSIZE,
		w->recursive,
		NOTIF_ALL,
		NULL,
		&w->ol,
		NULL,
		ReadDirectoryNotifyExtendedInformation);
	if (!b)
		w->was_error = 1; // could not reissue read

	return 0;
#else
	return -1;
#endif // LS_WINDOWS
}
