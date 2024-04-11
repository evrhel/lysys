#include <lysys/ls_watch.h>

#include <lysys/ls_core.h>

#include <string.h>
#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>

#include "ls_handle.h"
#include "ls_native.h"

#if LS_WINDOWS
#define NOTIF_BUFSIZE 1024

#elif LS_DARWIN
#include "ls_watch_darwin.h"

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

#if !LS_DARWIN
struct ls_watch_event_imp
{
	int action;

#if LS_WINDOWS
	char source[MAX_PATH];
	char target[MAX_PATH];
#else
	char *source;
	char *target;
#endif // LS_WINDOWS
	struct ls_watch_event_imp *next;
};
#endif

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
#elif LS_DARWIN
    ls_watch_darwin_t impl;
#else
	int notify; // inotify instance
	int watch; // inotify watch

	int recursive;

	pthread_t thread; // thread that created this watch
	volatile int stop; // stop flag
	volatile int running;

	pthread_mutex_t lock;
	pthread_cond_t cond;

	char avail[NOTIF_BUFSIZE];
	size_t avail_size;

	char source[NAME_MAX];
	char target[NAME_MAX];

	struct ls_watch_event_imp *front, *back;
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

#elif !LS_DARWIN

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
	struct notif n;
	ssize_t bytes_read;
	struct ls_watch_event_imp *e = NULL;

	for (;;)
	{
		pthread_mutex_lock(&w->lock);
		if (w->stop)
		{
			pthread_mutex_unlock(&w->lock);
			break;
		}
		pthread_mutex_unlock(&w->lock);

		bytes_read = read_event(w, &n);
		if (bytes_read <= 0)
			break;

		// require name
		if (!n.u.ie.len)
			break;

		e = ls_calloc(1, sizeof(struct ls_watch_event_imp));
		if (!e)
			break;
		
		if (n.u.ie.mask & IN_CREATE)
		{
			e->action = LS_WATCH_ADD;
			e->source = ls_strdup(n.u.ie.name);
			if (!e->source)
				break;
		}
		else if (n.u.ie.mask & IN_DELETE)
		{
			e->action = LS_WATCH_REMOVE;
			e->source = ls_strdup(n.u.ie.name);
			if (!e->source)
				break;
		}
		else if (n.u.ie.mask & IN_MODIFY)
		{
			e->action = LS_WATCH_MODIFY;
			e->source = ls_strdup(n.u.ie.name);
			if (!e->source)
				break;
		}
		else if (n.u.ie.mask & IN_MOVE)
		{
			e->action = LS_WATCH_RENAME;
			if (n.u.ie.mask & IN_MOVED_FROM)
			{
				e->source = ls_strdup(n.u.ie.name);
				if (!e->source)
					break;
			}
			else
			{
				e->source = ls_strdup(n.u.ie.name);
				if (!e->source)
					break;
			}

			bytes_read = read_event(w, &n);
			if (bytes_read <= 0)
				break;

			if (n.u.ie.mask & IN_MOVED_FROM)
			{
				e->target = ls_strdup(n.u.ie.name);
				if (!e->target)
					break;
			}
			else
			{
				e->target = ls_strdup(n.u.ie.name);
				if (!e->target)
					break;
			}
		}
		else
			ls_free(e), e = NULL;

		// add event to queue
		if (e)
		{
			pthread_mutex_lock(&w->lock);

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

			pthread_cond_signal(&w->cond);
			pthread_mutex_unlock(&w->lock);
		
			e = NULL;
		}
	}


	if (e)
	{
		if (e->source) ls_free(e->source);
		if (e->target) ls_free(e->target);
		ls_free(e);
	}
	
	pthread_mutex_lock(&w->lock);
	w->running = 0;
	pthread_cond_broadcast(&w->cond);
	pthread_mutex_unlock(&w->lock);

	return NULL;
}

#endif // LS_WINDOWS

static void LS_CLASS_FN ls_watch_dtor(struct ls_watch *w)
{
#if LS_WINDOWS
	if (w->hDirectory && w->hDirectory != INVALID_HANDLE_VALUE)
		CloseHandle(w->hDirectory);
#elif LS_DARWIN
    ls_watch_darwin_free(w->impl);
#else
	struct ls_watch_event_imp *e, *next;

	pthread_mutex_lock(&w->lock);
	w->stop = 1;
	pthread_mutex_unlock(&w->lock);

	pthread_kill(w->thread, SIGINT); // interrupt blocking calls
	inotify_rm_watch(w->notify, w->watch);
	close(w->notify);

	pthread_join(w->thread, NULL);
	
	pthread_cond_destroy(&w->cond);
	pthread_mutex_destroy(&w->lock);

	// clean up event queue
	e = w->front;
	while (e)
	{
		next = e->next;
		if (e->source) ls_free(e->source);
		if (e->target) ls_free(e->target);
		ls_free(e);
		e = next;
	}
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
#elif LS_DARWIN
    return ls_watch_darwin_wait(w->impl, ms);
#else
	int rc;
	struct timespec ts;
	
	rc = pthread_mutex_lock(&w->lock);
	if (rc == -1)
		return -1;

	rc = 0;
	while (!w->front && w->running)
	{
		if (ms == LS_INFINITE)
		{
			rc = pthread_cond_wait(&w->cond, &w->lock);
			if (rc != 0)
			{
				rc = -1;
				break;
			}
		}
		else
		{
			ts.tv_sec = ms / 1000;
			ts.tv_nsec = (ms % 1000) * 1000000;

			rc = pthread_cond_timedwait(&w->cond, &w->lock, &ts);
			if (rc == ETIMEDOUT)
			{
				rc = 1;
				break;
			}
			else if (rc != 0)
			{
				rc = -1;
				break;
			}
		}
	}

	pthread_mutex_unlock(&w->lock);

	return rc;
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
#elif LS_DARWIN
    struct ls_watch *w;
    
    w = ls_handle_create(&WatchClass);
    if (!w)
        return NULL;
    
    w->impl = ls_watch_darwin_alloc(dir, recursive);
    if (!w->impl)
    {
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

	w->recursive = !!recursive;

	rc = pthread_mutex_init(&w->lock, NULL);
	if (rc == -1)
	{
		ls_handle_dealloc(w);
		return NULL;
	}

	rc = pthread_cond_init(&w->cond, NULL);
	if (rc == -1)
	{
		pthread_mutex_destroy(&w->lock);
		ls_handle_dealloc(w);
		return NULL;
	}

	w->notify = inotify_init1(IN_CLOEXEC);
	if (w->notify == -1)
	{
		pthread_cond_destroy(&w->cond);
		pthread_mutex_destroy(&w->lock);
		ls_handle_dealloc(w);
		return NULL;
	}

	// use absolute path
	w->watch = inotify_add_watch(w->notify, dir, IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVE);
	if (w->watch == -1)
	{
		close(w->notify);
		pthread_cond_destroy(&w->cond);
		pthread_mutex_destroy(&w->lock);
		ls_handle_dealloc(w);
		return NULL;
	}

	// set close-on-exec flag
	rc = fcntl(w->watch, F_SETFD, FD_CLOEXEC);
	if (rc == -1)
	{
		inotify_rm_watch(w->notify, w->watch);
		close(w->notify);
		pthread_cond_destroy(&w->cond);
		pthread_mutex_destroy(&w->lock);
		ls_handle_dealloc(w);
		return NULL;
	}

	w->running = 1;
	
	rc = pthread_create(&w->thread, NULL, &ls_watch_thread, w);
	if (rc == -1)
	{
		w->running = 0;
		inotify_rm_watch(w->notify, w->watch);
		close(w->notify);
		pthread_cond_destroy(&w->cond);
		pthread_mutex_destroy(&w->lock);
		ls_handle_dealloc(w);
		return NULL;
	}

	return w;
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
#elif LS_DARWIN
    struct ls_watch *w = watch;
    return ls_watch_darwin_get_result(w->impl, event);
#else
	struct ls_watch *w = watch;
	struct ls_watch_event_imp *e;

	event->source = NULL;
	event->target = NULL;

	pthread_mutex_lock(&w->lock);

	if (!w->front)
	{
		pthread_mutex_unlock(&w->lock);
		return 1;
	}

	e = w->front;
	w->front = w->front->next;
	if (!w->front)
		w->back = NULL;

	if (e->source)
	{
		strncpy(w->source, e->source, NAME_MAX);
		event->source = w->source;
	}

	if (e->target)
	{
		strncpy(w->target, e->target, NAME_MAX);
		event->target = w->target;
	}

	pthread_mutex_unlock(&w->lock);

	event->action = e->action;

	if (e->target) ls_free(e->target);
	if (e->source) ls_free(e->source);
	ls_free(e);

	return 0;
#endif // LS_WINDOWS
}
