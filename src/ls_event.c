#include <lysys/ls_event.h>

#include <time.h>

#include <lysys/ls_core.h>

#include "ls_native.h"
#include "ls_handle.h"

struct event
{
#if LS_WINDOWS
    HANDLE hEvent;
#else
    pthread_mutex_t m;
    pthread_cond_t c;
    int signaled;
#endif // LS_WINDOWS
};

static void LS_CLASS_FN ls_event_dtor(struct event *evt)
{
#if LS_WINDOWS
    CloseHandle(evt->hEvent);
#else
    pthread_cond_destroy(&evt->c);
    pthread_mutex_destroy(&evt->m);
#endif // LS_WINDOWS
}

static int LS_CLASS_FN ls_event_wait(struct event *evt, unsigned long ms)
{
#if LS_WINDOWS
    DWORD dwResult;

	dwResult = WaitForSingleObject(*phEvent, ms);
	if (dwResult == WAIT_OBJECT_0) return 0;
    if (dwResult == WAIT_TIMEOUT) return 1;

	return -1;
#else
    int rc;
    struct timespec ts;

    rc = pthread_mutex_lock(&evt->m);
    if (rc != 0)
        return -1;

    while (!evt->signaled)
    {
        if (ms == LS_INFINITE)
            rc = pthread_cond_wait(&evt->c, &evt->m);
        else
        {
            ts.tv_sec = ms / 1000;
            ts.tv_nsec = (ms % 1000) * 1000000;

            rc = pthread_cond_timedwait(&evt->c, &evt->m, &ts);
        }
        
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

    pthread_mutex_unlock(&evt->m);

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
	phEvent = ls_handle_create(&EventClass);
	if (!phEvent) return NULL;
	*phEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
	return !*phEvent ? (ls_close(phEvent), NULL) : phEvent;
#else
    struct event *evt;
    int rc;
    evt = ls_handle_create(&EventClass);
    if (!evt) return NULL;

    rc = pthread_mutex_init(&evt->m, NULL);
    if (rc != 0)
    {
        ls_handle_dealloc(evt);
        return NULL;
    }
    
    rc = pthread_cond_init(&evt->c, NULL);
    if (rc != 0)
    {
        pthread_mutex_destroy(&evt->m);
        ls_handle_dealloc(evt);
        return NULL;
    }
    
    return evt;
#endif // LS_WINDOWS
}

int ls_event_signaled(ls_handle evt)
{
#if LS_WINDOWS
	return WaitForSingleObject(*(PHANDLE)evt, 0) == WAIT_OBJECT_0;
#else
    struct event *event = evt;
    int rc;
    
    pthread_mutex_lock(&event->m);
    rc = event->signaled;
    pthread_mutex_unlock(&event->m);
    
    return rc;
#endif // LS_WINDOWS
}

int ls_event_set(ls_handle evt)
{
#if LS_WINDOWS
	return !SetEvent(*(PHANDLE)evt);
#else
    struct event *event = evt;
    int rc;
    
    pthread_mutex_lock(&event->m);
    event->signaled = 1;
    pthread_mutex_unlock(&event->m);
    
    return 0;
#endif // LS_WINDOWS
}

int ls_event_reset(ls_handle evt)
{
#if LS_WINDOWS
	return !ResetEvent(*(PHANDLE)evt);
#else
    struct event *event = evt;
    int rc;
    
    pthread_mutex_lock(&event->m);
    event->signaled = 0;
    pthread_mutex_unlock(&event->m);
    
    return 0;
#endif // LS_WINDOWS
}
