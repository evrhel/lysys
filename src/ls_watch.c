#include <lysys/ls_watch.h>

#include <lysys/ls_core.h>
#include <lysys/ls_string.h>

#include <string.h>
#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <memory.h>

#include "ls_handle.h"
#include "ls_native.h"
#include "ls_sync_util.h"

#if LS_WINDOWS
#define NOTIF_FILTER (FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_SECURITY)
#define NOTIF_MAXSIZE (offsetof(FILE_NOTIFY_EXTENDED_INFORMATION, FileName) + MAX_PATH * sizeof(WCHAR))
#define NOTIF_BUFSIZE 2048 // must be at least NOTIF_MAXSIZE*2
#define BACKLOG 10

#elif LS_DARWIN
#else
#ifndef NAME_MAX
#define NAME_MAX 255
#endif // NAME_MAX

#define NOTIF_BUFSIZE (sizeof(struct inotify_event) + NAME_MAX + 1)
#define NOTIF_MINSIZE (sizeof(struct inotify_event))

struct notif
{
	union
	{
		struct inotify_event ie;
		char buf[NOTIF_BUFSIZE];
	} u;
};

#endif // LS_WINDOWS

struct ls_watch_event_imp
{
	struct ls_watch_event_imp *next;
	size_t cb; // size of event
	struct ls_watch_event event;
};

struct ls_watch
{
#if LS_WINDOWS
	OVERLAPPED ov; // THIS MUST BE FIRST!

	HANDLE hDirectory;
	HANDLE hThread;

	ls_lock_t lock;
	ls_cond_t cond;

	int error; // error code
	int avail; // number of events available

	BYTE buf[NOTIF_BUFSIZE];
	DWORD dwTransferred;

	int flags;

	struct ls_watch_event_imp *front, *back;
#elif LS_DARWIN
	FSEventStreamRef stream;
	dispatch_queue_t queue;
	ls_lock_t lock;
	ls_cond_t cond;
	
	int flags;
	
	int error;
	
	struct ls_watch_event_imp *front, *back;
#else
	int notify; // inotify instance
	int watch; // inotify watch

	int flags;

	pthread_t thread; // thread that created this watch
	int error;

	ls_lock_t lock;
	ls_cond_t cond;

	char avail[NOTIF_BUFSIZE];
	size_t avail_size;

	struct ls_watch_event_imp *front, *back;
#endif // LS_WINDOWS
};

#if LS_WINDOWS

// w->cs must be held
// set w->error before calling
static void ls_watch_emit_error(struct ls_watch *w)
{
	assert(w->error != 0);

	if (w->avail == -1)
		return;	

	w->avail = -1;
	cond_broadcast(&w->cond);
}

// includes null terminator
static char *ls_extract_name(PFILE_NOTIFY_EXTENDED_INFORMATION pNotify, size_t *len)
{
	WCHAR szName[MAX_PATH];
	char *name;
	size_t name_len;

	if (pNotify->FileNameLength >= sizeof(szName) - sizeof(WCHAR))
	{
		ls_set_errno(LS_BUFFER_TOO_SMALL);
		return NULL;
	}

	memcpy(szName, pNotify->FileName, pNotify->FileNameLength);
	szName[pNotify->FileNameLength / sizeof(WCHAR)] = L'\0';

	// determine required buffer size
	name_len = ls_wchar_to_utf8_buf(szName, NULL, 0);
	if (name_len == -1)
		return NULL;

	name = ls_malloc(name_len);
	if (!name)
		return NULL;

	// convert name
	name_len = ls_wchar_to_utf8_buf(szName, name, name_len);
	if (name_len == -1)
	{
		ls_free(name);
		return NULL;
	}

	*len = name_len + 1;
	return name;
}

// w->cs must be held
static void ls_enqueue_events(struct ls_watch *w)
{
	PFILE_NOTIFY_EXTENDED_INFORMATION pNotify, pNext;
	struct ls_watch_event_imp *e = NULL;
	size_t cb;

	char *first = NULL, *second = NULL;
	size_t first_len, second_len;

	_ls_errno = 0;

	if (w->avail == -1)
		return; // dont enqueue events if we are shutting down

	pNotify = (PFILE_NOTIFY_EXTENDED_INFORMATION)w->buf;
	pNext = (PFILE_NOTIFY_EXTENDED_INFORMATION)(w->buf + pNotify->NextEntryOffset);

	// convert names to null-terminated utf-8 strings

	first = ls_extract_name(pNotify, &first_len);
	if (!first)
		goto failure;

	// if NextEntryOffset is non-zero, there is a second notification
	if (pNotify->NextEntryOffset)
	{
		second = ls_extract_name(pNext, &second_len);
		if (!second)
			goto failure;
	}
	else
		second_len = 0;
	
	cb = offsetof(struct ls_watch_event_imp, event.filename) + first_len + second_len;
	e = ls_calloc(1, cb);
	if (!e)
		goto failure;
	e->cb = cb;

	// populate event
	switch (pNotify->Action)
	{
	case FILE_ACTION_ADDED:
		if (second)
			goto failure;
		e->event.type = LS_WATCH_ADD;
		memcpy(e->event.filename, first, first_len);
		break;
	case FILE_ACTION_REMOVED:
		if (second)
			goto failure;
		e->event.type = LS_WATCH_REMOVE;
		memcpy(e->event.filename, first, first_len);
		break;
	case FILE_ACTION_MODIFIED:
		if (second)
			goto failure;
		e->event.type = LS_WATCH_MODIFY;
		memcpy(e->event.filename, first, first_len);
		break;
	case FILE_ACTION_RENAMED_OLD_NAME:
		if (!second)
			goto failure;
		e->event.type = LS_WATCH_RENAME;
		e->event.old_name = second_len;
		memcpy(e->event.filename, second, second_len);
		memcpy(e->event.filename + second_len, first, first_len);
		break;
	case FILE_ACTION_RENAMED_NEW_NAME:
		if (!second)
			goto failure;
		e->event.type = LS_WATCH_RENAME;
		e->event.old_name = first_len;
		memcpy(e->event.filename, first, first_len);
		memcpy(e->event.filename + first_len, second, second_len);
		break;
	default:
		w->error = LS_INVALID_STATE;
		goto failure;
	}

	free(first), first = NULL;
	free(second), second = NULL;

	// add event to queue
	if (w->front)
	{
		w->back->next = e;
		w->back = e;
	}
	else
	{
		w->front = e;
		w->back = e;
	}

	// notify waiting threads
	w->avail++;
	cond_signal(&w->cond);

	return;
failure:
	if (!w->error)
	{
		w->error = _ls_errno;
		if (!w->error)
			w->error = LS_INVALID_STATE;
	}

	ls_free(first);
	ls_free(second);
	ls_free(e);
	ls_watch_emit_error(w);
}

static void ls_completion_routine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
{
	struct ls_watch *w;
	BOOL b;

	w = (struct ls_watch *)lpOverlapped;

	lock_lock(&w->lock);

	// TODO: should we do this?
	// SetEvent(w->ov.hEvent);

	if (dwErrorCode != ERROR_SUCCESS)
	{
		w->error = win32_to_error(dwErrorCode);
		ls_watch_emit_error(w);
		lock_unlock(&w->lock);
		return;
	}
	
	b = GetOverlappedResult(w->hDirectory, lpOverlapped, &w->dwTransferred, FALSE);
	if (b)
	{
		ls_enqueue_events(w);
		lock_unlock(&w->lock);
		return;
	}
		
	w->error = win32_to_error(GetLastError());
	ls_watch_emit_error(w);

	lock_unlock(&w->lock);
}

static DWORD CALLBACK ls_watch_worker(LPVOID lpParam)
{
	struct ls_watch *w;
	BOOL b;
	DWORD dwResult;
	DWORD dwErr;
	
	w = lpParam;

	for (;;)
	{
		ResetEvent(w->ov.hEvent);

		// queue next read
		b = ReadDirectoryChangesExW(
			w->hDirectory,
			w->buf,
			NOTIF_BUFSIZE,
			!!(w->flags & LS_WATCH_FLAG_RECURSIVE),
			NOTIF_FILTER,
			NULL,
			&w->ov,
			&ls_completion_routine,
			ReadDirectoryNotifyExtendedInformation);
		if (!b)
		{
			dwErr = GetLastError();
			if (dwErr != ERROR_IO_PENDING)
			{
				if (dwErr == ERROR_NOTIFY_ENUM_DIR)
				{
					// TODO: need to traverse directory manually
					lock_lock(&w->lock);
					w->error = LS_NOT_IMPLEMENTED;
					ls_watch_emit_error(w);
					lock_unlock(&w->lock);
					break;
				}

				lock_lock(&w->lock);
				w->error = win32_to_error(dwErr);
				ls_watch_emit_error(w);
				lock_unlock(&w->lock);
				break;
			}
		}

		// alertable wait for I/O completion
		dwResult = WaitForSingleObjectEx(w->ov.hEvent, INFINITE, TRUE);
		dwErr = GetLastError();

		lock_lock(&w->lock);

		switch (dwResult)
		{
		case WAIT_IO_COMPLETION:
			break; // handled by completion routine
		case WAIT_FAILED:
			w->error = win32_to_error(dwErr);
			ls_watch_emit_error(w);
			break;
		case WAIT_TIMEOUT:
			w->error = LS_TIMEDOUT;
			ls_watch_emit_error(w);
			break;
		default:
			w->error = LS_INVALID_STATE;
			ls_watch_emit_error(w);
			break;
		}

		// check if we should exit
		if (w->avail == -1)
		{
			lock_unlock(&w->lock);
			break;
		}

		lock_unlock(&w->lock);
	}

	return 0;
}

#elif LS_DARWIN

static void ls_on_fs_event(ConstFSEventStreamRef streamRef, void *clientCallBackInfo,
	size_t numEvents, void *eventPaths, const FSEventStreamEventFlags *eventFlags,
	const FSEventStreamEventId *eventIds)
{
	struct ls_watch *w = clientCallBackInfo;
	size_t i;
	char **paths = eventPaths;
	
	for (i = 0; i < numEvents; i++)
	{
		// TODO: need to find the actual change
	}
	
	// TODO: remove when implemented
	lock_lock(&w->lock);
	w->error = LS_NOT_IMPLEMENTED;
	cond_broadcast(&w->cond);
	lock_unlock(&w->lock);
}

#else

static ssize_t read_avail(struct ls_watch *w, void *buf, size_t n)
{
	ssize_t to_copy;

	to_copy = w->avail_size < n ? w->avail_size : n;
	memcpy(buf, w->avail, to_copy);
	w->avail_size -= to_copy;
	memmove(w->avail, w->avail + to_copy, w->avail_size);

	return to_copy;
}

static ssize_t read_bytes(struct ls_watch *w, void *buf, size_t cb)
{
	ssize_t bytes_read;
	ssize_t total_read = 0;
	size_t max_read;

	total_read += read_avail(w, buf, cb);
	if (total_read == cb)
		return cb;

	max_read = NOTIF_BUFSIZE - w->avail_size;
	bytes_read = read(w->notify, w->avail + w->avail_size, max_read);
	if (bytes_read <= 0)
		return bytes_read;
	w->avail_size += bytes_read;

	total_read += read_avail(w, (uint8_t *)buf + total_read, cb - total_read);
	
	return total_read;
}

static ssize_t read_event(struct ls_watch *w, struct notif *pn)
{
	ssize_t bytes_read;
	size_t total_bytes = 0;

	bytes_read = read_bytes(w, pn->u.buf, NOTIF_MINSIZE);
	if (bytes_read <= 0)
		return -1;
	total_bytes += bytes_read;

	if (bytes_read < NOTIF_MINSIZE)
		return -1;

	// read name if present
	if (pn->u.ie.len)
	{
		bytes_read = read_bytes(w, pn->u.ie.name, pn->u.ie.len);
		if (bytes_read <= 0)
			return -1;
		total_bytes += bytes_read;
	}

	return total_bytes;
}

static void *ls_watch_thread(void *arg)
{
	struct ls_watch *w = arg;
	struct notif n, n2;
	ssize_t bytes_read;
	struct ls_watch_event_imp *e = NULL, *tmp;
	size_t cb;

	for (;;)
	{
		lock_lock(&w->lock);
		if (w->error)
		{
			lock_unlock(&w->lock);
			break;
		}
		lock_unlock(&w->lock);

		bytes_read = read_event(w, &n);
		if (bytes_read <= 0)
			break;

		// require name
		if (!n.u.ie.len)
			break;

		cb = sizeof(struct ls_watch_event_imp) + n.u.ie.len;
		e = ls_calloc(1, cb);
		if (!e)
			break;
		
		if (n.u.ie.mask & IN_CREATE)
		{
			e->event.type = LS_WATCH_ADD;
			memcpy(e->event.filename, n.u.ie.name, n.u.ie.len);
		}
		else if (n.u.ie.mask & IN_DELETE)
		{
			e->event.type = LS_WATCH_REMOVE;
			memcpy(e->event.filename, n.u.ie.name, n.u.ie.len);
		}
		else if (n.u.ie.mask & IN_MODIFY)
		{
			e->event.type = LS_WATCH_MODIFY;
			memcpy(e->event.filename, n.u.ie.name, n.u.ie.len);
		}
		else if (n.u.ie.mask & IN_MOVE)
		{
			bytes_read = read_event(w, &n2);
			if (bytes_read <= 0)
				ls_free(e), e = NULL;

			cb = sizeof(struct ls_watch_event_imp) + n.u.ie.len + n2.u.ie.len;
			tmp = ls_realloc(e, cb);
			if (!tmp)
				break;
			e = tmp, tmp = NULL;

			e->event.type = LS_WATCH_RENAME;

			if (n.u.ie.mask & IN_MOVED_FROM)
			{
				if (n2.u.ie.mask & IN_MOVED_TO)
				{
					memcpy(e->event.filename, n.u.ie.name, n.u.ie.len);
					memcpy(e->event.filename + n.u.ie.len, n2.u.ie.name, n2.u.ie.len);
					e->event.old_name = n.u.ie.len;
				}
				else
					ls_free(e), e = NULL;
			}
			else if (n.u.ie.mask & IN_MOVED_TO)
			{
				if (n2.u.ie.mask & IN_MOVED_TO)
				{
					memcpy(e->event.filename, n2.u.ie.name, n2.u.ie.len);
					memcpy(e->event.filename + n2.u.ie.len, n.u.ie.name, n.u.ie.len);
					e->event.old_name = n2.u.ie.len;
				}
				else
					ls_free(e), e = NULL;
			}
			else
				ls_free(e), e = NULL;
		}
		else
			ls_free(e), e = NULL;

		// add event to queue
		if (e)
		{
			e->cb = cb;

			lock_lock(&w->lock);

			if (!w->front)
			{
				w->front = e;
				w->back = e;
			}
			else
			{
				w->back->next = e;
				w->back = e;
			}

			cond_signal(&w->cond);
			lock_unlock(&w->lock);
		
			e = NULL;
		}
	}

	ls_free(e);

	return NULL;
}

#endif // LS_WINDOWS

static void ls_watch_dtor(struct ls_watch *w)
{
#if LS_WINDOWS
	DWORD dwRet;

	// cancel pending I/O
	CancelIoEx(w->hDirectory, &w->ov);

	// signal to stop (should be done in worker thread, just in case)
	lock_lock(&w->lock);
	w->error = LS_CANCELED;
	ls_watch_emit_error(w);
	lock_unlock(&w->lock);

	// wait for worker thread to exit
	dwRet = WaitForSingleObject(w->hThread, INFINITE);

	CloseHandle(w->hThread);
	CloseHandle(w->hDirectory);
	CloseHandle(w->ov.hEvent);

	lock_unlock(&w->lock);
#elif LS_DARWIN
	FSEventStreamRelease(w->stream);
	dispatch_release(w->queue);
	cond_destroy(&w->cond);
	lock_destroy(&w->lock);
	ls_free(w);
#else
	struct ls_watch_event_imp *e, *next;

	lock_lock(&w->lock);
	w->error = LS_CANCELED;
	cond_broadcast(&w->cond);
	lock_unlock(&w->lock);

	pthread_kill(w->thread, SIGINT); // interrupt blocking calls
	inotify_rm_watch(w->notify, w->watch);
	close(w->notify);

	pthread_join(w->thread, NULL);
	
	cond_destroy(&w->cond);
	lock_destroy(&w->lock);

	// clean up event queue
	e = w->front;
	while (e)
	{
		next = e->next;
		ls_free(e);
		e = next;
	}
#endif // LS_WINDOWS
}

static int ls_watch_wait(struct ls_watch *w, unsigned long ms)
{
#if LS_WINDOWS
	int rc;

	lock_lock(&w->lock);

	while (w->avail == 0)
	{
		rc = cond_wait(&w->cond, &w->lock, ms);
		if (rc == 1)
		{
			lock_unlock(&w->lock);
			return 1; // timeout
		}
	}

	if (w->avail == -1)
	{
		rc = w->error;
		lock_unlock(&w->lock);
		return ls_set_errno(rc);
	}

	lock_unlock(&w->lock);

	return 0;
#else
	int rc;
	
	lock_lock(&w->lock);

	while (!w->front && !w->error)
	{
		rc = cond_wait(&w->cond, &w->lock, ms);
		if (rc == 1)
		{
			lock_unlock(&w->lock);
			return 1;
		}
	}
	
	if (w->error)
	{
		rc = w->error;
		lock_unlock(&w->lock);
		return ls_set_errno(rc);
	}

	lock_unlock(&w->lock);

	return 0;
#endif // LS_WINDOWS
}

static const struct ls_class WatchClass = {
	.type = LS_WATCH,
	.cb = sizeof(struct ls_watch),
	.dtor = (ls_dtor_t)&ls_watch_dtor,
	.wait = (ls_wait_t)&ls_watch_wait
};

ls_handle ls_watch_dir(const char *dir, int flags)
{
#if LS_WINDOWS
	struct ls_watch *w;
	WCHAR szPath[MAX_PATH];
	DWORD dwErr;

	w = ls_handle_create(&WatchClass);
	if (!w)
		return NULL;

	if (ls_utf8_to_wchar_buf(dir, szPath, MAX_PATH) == -1)
	{
		ls_handle_dealloc(w);
		return NULL;
	}

	w->flags = flags;

	w->hDirectory = CreateFileW(szPath, FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (w->hDirectory == INVALID_HANDLE_VALUE)
	{
		dwErr = GetLastError();
		ls_handle_dealloc(w);
		ls_set_errno_win32(dwErr);
		return NULL;
	}

	w->ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
	if (!w->ov.hEvent)
	{
		dwErr = GetLastError();
		CloseHandle(w->hDirectory);
		ls_handle_dealloc(w);
		ls_set_errno_win32(dwErr);
		return NULL;
	}

	(void)lock_init(&w->lock);

	w->hThread = CreateThread(NULL, 0, &ls_watch_worker, w, 0, NULL);
	if (!w->hThread)
	{
		dwErr = GetLastError();
		lock_destroy(&w->lock);
		CloseHandle(w->ov.hEvent);
		CloseHandle(w->hDirectory);
		ls_handle_dealloc(w);
		ls_set_errno_win32(dwErr);
		return NULL;
	}

	return w;
#elif LS_DARWIN
	struct ls_watch *w;
	int rc;
	FSEventStreamContext ctx;
	CFStringRef dirsr;
	CFArrayRef paths;
	Boolean b;
	
	w = ls_handle_create(&WatchClass);
	if (!w)
		return NULL;
	
	rc = lock_init(&w->lock);
	if (rc == -1)
	{
		ls_handle_dealloc(w);
		return NULL;
	}
	
	rc = cond_init(&w->cond);
	if (rc == -1)
	{
		lock_destroy(&w->lock);
		ls_handle_dealloc(w);
		return NULL;
	}
	
	dirsr = CFStringCreateWithCString(NULL, dir, kCFStringEncodingUTF8);
	if (!dirsr)
	{
		cond_destroy(&w->cond);
		lock_destroy(&w->lock);
		ls_handle_dealloc(w);
		return NULL;
	}
	
	paths = CFArrayCreate(NULL, (const void **)&dirsr, 1, &kCFTypeArrayCallBacks);
	if (!paths)
	{
		CFRelease(dirsr);
		cond_destroy(&w->cond);
		lock_destroy(&w->lock);
		ls_handle_dealloc(w);
		return NULL;
	}
	
	CFRelease(dirsr), dirsr = NULL;

	ctx.version = 0;
	ctx.info = w;
	ctx.retain = NULL;
	ctx.release = NULL;
	ctx.copyDescription = NULL;
	
	// create event queue
	w->queue = dispatch_queue_create(dir, DISPATCH_QUEUE_CONCURRENT);
	if (!w->queue)
	{
		cond_destroy(&w->cond);
		lock_destroy(&w->lock);
		ls_handle_dealloc(w);
		return NULL;
	}
   
	// Create event stream
	w->stream = FSEventStreamCreate(NULL,
									&ls_on_fs_event,
									&ctx,
									paths,
									kFSEventStreamEventIdSinceNow,
									0.1,
									kFSEventStreamCreateFlagNone);
	
	CFRelease(paths), paths = NULL;
	
	if (!w->stream)
	{
		dispatch_release(w->queue);
		cond_destroy(&w->cond);
		lock_destroy(&w->lock);
		ls_handle_dealloc(w);
		return NULL;
	}
	
	// set the event queue for the fs stream
	FSEventStreamSetDispatchQueue(w->stream, w->queue);
	
	// start recieving events
	b = FSEventStreamStart(w->stream);
	if (!b)
	{
		FSEventStreamRelease(w->stream);
		dispatch_release(w->queue);
		cond_destroy(&w->cond);
		lock_destroy(&w->lock);
		ls_handle_dealloc(w);
		return NULL;
	}
	
	return w;
	
#else
	struct ls_watch *w;
	int rc;

	w = ls_handle_create(&WatchClass);
	if (!w)
		return NULL;

	w->flags = flags;

	rc = lock_init(&w->lock);
	if (rc == -1)
	{
		ls_handle_dealloc(w);
		return NULL;
	}

	rc = cond_init(&w->cond);
	if (rc == -1)
	{
		lock_destroy(&w->lock);
		ls_handle_dealloc(w);
		return NULL;
	}

	w->notify = inotify_init1(IN_CLOEXEC);
	if (w->notify == -1)
	{
		rc = errno;
		cond_destroy(&w->cond);
		lock_destroy(&w->lock);
		ls_handle_dealloc(w);
		ls_set_errno_errno(rc);
		return NULL;
	}

	// use absolute path
	w->watch = inotify_add_watch(w->notify, dir, IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVE);
	if (w->watch == -1)
	{
		rc = errno;
		close(w->notify);
		cond_destroy(&w->cond);
		lock_destroy(&w->lock);
		ls_handle_dealloc(w);
		ls_set_errno_errno(rc);
		return NULL;
	}

	// set close-on-exec flag
	rc = fcntl(w->watch, F_SETFD, FD_CLOEXEC);
	if (rc == -1)
	{
		rc = errno;
		inotify_rm_watch(w->notify, w->watch);
		close(w->notify);
		cond_destroy(&w->cond);
		lock_destroy(&w->lock);
		ls_handle_dealloc(w);
		ls_set_errno_errno(rc);
		return NULL;
	}

	rc = pthread_create(&w->thread, NULL, &ls_watch_thread, w);
	if (rc == -1)
	{
		rc = errno;
		inotify_rm_watch(w->notify, w->watch);
		close(w->notify);
		cond_destroy(&w->cond);
		lock_destroy(&w->lock);
		ls_handle_dealloc(w);
		ls_set_errno_errno(rc);
		return NULL;
	}

	return w;
#endif // LS_WINDOWS
}

size_t ls_watch_get_result(ls_handle watch, struct ls_watch_event *event, size_t cb)
{
	int error;
	struct ls_watch *w;
	struct ls_watch_event_imp *e;
	size_t len;

	w = watch;

	if (!w)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	if (!event != !cb)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	lock_lock(&w->lock);

	if (w->error != 0)
	{
		error = w->error;
		lock_unlock(&w->lock);
		return ls_set_errno(error);
	}

	e = w->front;

	if (!e)
	{
		// no events
		lock_unlock(&w->lock);
		return 0;
	}

	len = e->cb;

	if (!event)
	{
		lock_unlock(&w->lock);
		return len;
	}

	if (len < cb)
	{
		lock_unlock(&w->lock);
		return ls_set_errno(LS_BUFFER_TOO_SMALL);
	}

	memcpy(event, &e->event, len);

	w->front = w->front->next;
	if (!w->front)
		w->back = NULL;

	lock_unlock(&w->lock);

	ls_free(e);

	return len;
}
