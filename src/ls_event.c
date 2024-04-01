#include <lysys/ls_event.h>

#include <lysys/ls_core.h>

#include "ls_native.h"
#include "ls_handle.h"

#if LS_WINDOWS
static void LS_CLASS_FN ls_event_dtor(PHANDLE phEvent)
{
	CloseHandle(*phEvent);
}

static int LS_CLASS_FN ls_event_wait(PHANDLE phEvent)
{
	DWORD dwResult;

	dwResult = WaitForSingleObject(*phEvent, INFINITE);
	if (dwResult == WAIT_OBJECT_0) return 0;

	return -1;
}
#else

struct event
{
    pthread_mutex_t m;
    pthread_cond_t c;
    int signaled;
};

static void LS_CLASS_FN ls_event_dtor(struct event *evt)
{
    if (evt->c)
        pthread_cond_destroy(&evt->c);
    
    if (evt->m)
        pthread_mutex_destroy(&evt->m);
}

static void LS_CLASS_FN ls_event_wait(struct event *evt)
{
    pthread_mutex_lock(&evt->m);
    while (!evt->signaled)
    {
        pthread_cond_wait(&evt->c, &evt->m);
        pthread_mutex_lock(&evt->m);
    }
    pthread_mutex_unlock(&evt->m);
}

#endif // LS_WINDOWS

static struct ls_class EventClass = {
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
        ls_close(evt);
        return NULL;
    }
    
    rc = pthread_cond_init(&evt->c, NULL);
    if (rc != 0)
    {
        ls_close(evt);
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
