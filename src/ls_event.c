#include <lysys/ls_event.h>

#include <time.h>

#include <lysys/ls_core.h>

#include "ls_native.h"
#include "ls_handle.h"
#include "ls_sync_util.h"

struct event
{
#if LS_WINDOWS
    HANDLE hEvent;
#else
    ls_lock_t lock;
    ls_cond_t cond;
    int signaled;
#endif // LS_WINDOWS
};

static void ls_event_dtor(struct event *evt)
{
#if LS_WINDOWS
    CloseHandle(evt->hEvent);
#else
    cond_destroy(&evt->cond);
    lock_destroy(&evt->lock);
#endif // LS_WINDOWS
}

static int ls_event_wait(struct event *evt, unsigned long ms)
{
#if LS_WINDOWS
    DWORD dwResult;

	dwResult = WaitForSingleObject(evt->hEvent, ms);

	if (dwResult == WAIT_OBJECT_0)
        return 0;

    if (dwResult == WAIT_TIMEOUT)
        return 1;

	return ls_set_errno_win32(GetLastError());
#else
    int rc;
    struct timespec ts;

    lock_lock(&evt->lock);

    while (!evt->signaled)
        cond_wait(&evt->cond, &evt->lock, ms);

    lock_unlock(&evt->lock);

    return rc;
#endif // LS_WINDOWS
}
static const struct ls_class EventClass = {
	.type = LS_EVENT,
#if LS_WINDOWS
	.cb = sizeof(HANDLE),
#else
    .cb = sizeof(struct event),
#endif // LS_WINDOWS
	.dtor = (ls_dtor_t)&ls_event_dtor,
	.wait = (ls_wait_t)&ls_event_wait
};

ls_handle ls_event_create(void)
{
#if LS_WINDOWS
    PHANDLE phEvent;
	HANDLE hEvent;

    phEvent = ls_handle_create(&EventClass);
	if (!phEvent)
        return NULL;

	hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!hEvent)
    {
        ls_set_errno_win32(GetLastError());
        ls_handle_dealloc(hEvent);
        return NULL;
    }

    *phEvent = hEvent;
    return phEvent;
#else
    struct event *evt;
    int rc;

    evt = ls_handle_create(&EventClass);
    if (!evt)
        return NULL;

    rc = lock_init(&evt->lock);
    if (rc != 0)
    {
        ls_handle_dealloc(evt);
        return NULL;
    }
    
    rc = cond_init(&evt->cond);
    if (rc != 0)
    {
        lock_destroy(&evt->lock);
        ls_handle_dealloc(evt);
        return NULL;
    }
    
    return evt;
#endif // LS_WINDOWS
}

int ls_event_signaled(ls_handle evt)
{
#if LS_WINDOWS
    PHANDLE phEvent = evt;

    if (!phEvent)
        return ls_set_errno(LS_INVALID_ARGUMENT);

    return WaitForSingleObject(*phEvent, 0) == WAIT_OBJECT_0;
#else
    struct event *event = evt;
    int rc;

    if (!evt)
        return ls_set_errno(LS_INVALID_HANDLE);
    
    lock_lock(&event->lock);
    rc = event->signaled;
    lock_unlock(&event->lock);
    
    return rc;
#endif // LS_WINDOWS
}

int ls_event_set(ls_handle evt)
{
#if LS_WINDOWS
    BOOL b;
    PHANDLE phEvent = evt;

    if (!phEvent)
        return ls_set_errno(LS_INVALID_ARGUMENT);

	b = SetEvent(*phEvent);
    if (!b)
        return ls_set_errno_win32(GetLastError());
    return 0;
#else
    struct event *event = evt;
    int rc;

    if (!evt)
        return ls_set_errno(LS_INVALID_HANDLE);
    
    lock_lock(&event->lock);
    event->signaled = 1;
    cond_broadcast(&event->cond);
    lock_unlock(&event->lock);
    
    return 0;
#endif // LS_WINDOWS
}

int ls_event_reset(ls_handle evt)
{
#if LS_WINDOWS
    BOOL b;
    PHANDLE phEvent = evt;

    if (!phEvent)
        return ls_set_errno(LS_INVALID_ARGUMENT);

    b = ResetEvent(*phEvent);
    if (!b)
        return ls_set_errno_win32(GetLastError());
    return 0;
#else
    struct event *event = evt;
    int rc;

    if (!evt)
        return ls_set_errno(LS_INVALID_HANDLE);
    
    lock_lock(&event->lock);
    event->signaled = 0;
    lock_unlock(&event->lock);
    
    return 0;
#endif // LS_WINDOWS
}
