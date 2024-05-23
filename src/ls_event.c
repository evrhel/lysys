#include <lysys/ls_event.h>

#include <time.h>

#include <lysys/ls_core.h>

#include "ls_native.h"
#include "ls_handle.h"
#include "ls_sync_util.h"
#include "ls_event_priv.h"

static void ls_event_dtor(struct ls_event *ev)
{
#if LS_WINDOWS
    CloseHandle(ev->hEvent);
#else
    cond_destroy(&ev->cond);
    lock_destroy(&ev->lock);
#endif // LS_WINDOWS
}

static int ls_event_wait(struct ls_event *evt, unsigned long ms)
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
    .cb = sizeof(struct ls_event),
    .dtor = (ls_dtor_t)&ls_event_dtor,
    .wait = (ls_wait_t)&ls_event_wait
};

ls_handle ls_event_create(void)
{
#if LS_WINDOWS
    struct ls_event *ev;

    ev = ls_handle_create(&EventClass);
    if (!ev)
        return NULL;

    ev->hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!ev->hEvent)
    {
        ls_set_errno_win32(GetLastError());
        ls_handle_dealloc(ev);
        return NULL;
    }

    return ev;
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
    struct ls_event *ev = evt;

    if (!ev)
        return ls_set_errno(LS_INVALID_HANDLE);

    return WaitForSingleObject(ev->hEvent, 0) == WAIT_OBJECT_0;
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
    struct ls_event *ev = evt;

    if (!ev)
        return ls_set_errno(LS_INVALID_HANDLE);

    b = SetEvent(ev->hEvent);
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
    struct ls_event *ev = evt;

    if (!ev)
        return ls_set_errno(LS_INVALID_HANDLE);

    b = ResetEvent(ev->hEvent);
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
