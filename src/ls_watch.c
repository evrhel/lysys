#include <lysys/ls_watch.h>

#include <lysys/ls_core.h>

#include <string.h>
#include <assert.h>
#include <signal.h>

#include "ls_handle.h"
#include "ls_native.h"

#if LS_WINDOWS
#define NOTIF_BUFSIZE 1024
#else
#define NOTIF_BUFSIZE (offsetof(struct inotify_event, name))
#endif // LS_WINDOWS

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
	int notify; // inotify instance
	int flags; // inotify flags

	int recursive;

	pthread_t thread; // thread that created this watch
	volatile int stop; // stop flag
	volatile int running;

	pthread_mutex_t lock;
	pthread_cond_t cond;

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

#endif // LS_WINDOWS

#if LS_POSIX

static void *ls_watch_thread(void *arg)
{
	struct ls_watch *w = arg;
	char buf[NOTIF_BUFSIZE];
	struct inotify_event *ie = (struct inotify_event *)buf;
	int rc;
	ssize_t bytes_read;
	char *name = NULL;
	char *new_name;
	uint32_t name_len = 0;
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

		bytes_read = read(w->notify, buf, NOTIF_BUFSIZE);
		if (bytes_read <= 0)
			break;

		if (ie->len > name_len)
		{
			new_name = ls_realloc(name, ie->len);
			if (!new_name)
				break;
			name = new_name;

			name_len = ie->len;
		}

		pthread_mutex_lock(&w->lock);
		if (w->stop)
		{
			pthread_mutex_unlock(&w->lock);
			break;
		}
		pthread_mutex_unlock(&w->lock);

		bytes_read = read(w->notify, name, ie->len);
		if (bytes_read <= 0)
			break;

		e = ls_calloc(1, sizeof(struct ls_watch_event_imp));
		if (!e)
			break;
		
		if (ie->mask & IN_CREATE)
		{
			e->action = LS_WATCH_ADD;
			e->source = ls_strdup(name);
			if (!e->source)
				break;
		}
		else if (ie->mask & IN_DELETE)
		{
			e->action = LS_WATCH_REMOVE;
			e->source = ls_strdup(name);
			if (!e->source)
				break;
		}
		else if (ie->mask & IN_MODIFY)
		{
			e->action = LS_WATCH_MODIFY;
			e->source = ls_strdup(name);
			if (!e->source)
				break;
		}
		else if (ie->mask & IN_MOVE)
		{
			e->action = LS_WATCH_RENAME;
			ls_free(e), e = NULL;

			// TODO: implement rename
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

			pthread_cond_broadcast(&w->cond);
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

	if (name)
		ls_free(name);
	
	pthread_mutex_lock(&w->lock);
	w->running = 0;
	pthread_cond_broadcast(&w->cond);
	pthread_mutex_unlock(&w->lock);

	return NULL;
}

#endif // LS_POSIX

static void LS_CLASS_FN ls_watch_dtor(struct ls_watch *w)
{
#if LS_WINDOWS
	if (w->hDirectory && w->hDirectory != INVALID_HANDLE_VALUE)
		CloseHandle(w->hDirectory);
#else
	struct ls_watch_event_imp *e, *next;

	pthread_mutex_lock(&w->lock);
	w->stop = 1;
	pthread_mutex_unlock(&w->lock);

	pthread_kill(w->thread, SIGINT); // interrupt blocking calls
	close(w->notify);

	pthread_join(w->thread, NULL);
	
	pthread_cond_destroy(&w->cond);
	pthread_mutex_destroy(&w->lock);

	// clean up event queue
	e = w->front;
	while (e)
	{
		next = e->next;
		if (e->source) free(e->source);
		if (e->target) free(e->target);
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
#else
	int rc;
	struct timespec ts;
	unsigned long remain = ms;
	
	pthread_mutex_lock(&w->lock);
	
	while (!w->front && w->running)
	{
		if (ms == LS_INFINITE)
		{
			pthread_cond_wait(&w->cond, &w->lock);
			pthread_mutex_lock(&w->lock);
		}
		else
		{
			// TODO: implement timeout
			pthread_mutex_unlock(&w->lock);
			return 1;
		}
	}

	pthread_mutex_unlock(&w->lock);

	return 0;
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
	struct ls_watch *w;
	int rc;

	w = ls_handle_create(&WatchClass);
	if (!w) return NULL;

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

	w->running = 1;
	
	rc = pthread_create(&w->thread, NULL, &ls_watch_thread, w);
	if (rc == -1)
	{
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
#else
	struct ls_watch *w = watch;
	struct ls_watch_event_imp *e;

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

	pthread_mutex_unlock(&w->lock);

	event->action = e->action;
	event->source = e->source;
	event->target = e->target;

	ls_free(e);

	return 0;
#endif // LS_WINDOWS
}
