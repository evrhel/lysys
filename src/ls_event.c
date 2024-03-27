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

static void LS_CLASS_FN ls_event_dtor(void *dummy)
{
    // TODO: implement
}

static void LS_CLASS_FN ls_event_wait(void *dummy)
{
    // TODO: implement
}

#endif // LS_WINDOWS

static struct ls_class EventClass = {
	.type = LS_EVENT,
#if LS_WINDOWS
	.cb = sizeof(HANDLE),
#endif
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
#endif
}

int ls_event_signaled(ls_handle evt)
{
#if LS_WINDOWS
	return WaitForSingleObject(*(PHANDLE)evt, 0) == WAIT_OBJECT_0;
#endif
}

int ls_event_set(ls_handle evt)
{
#if LS_WINDOWS
	return !SetEvent(*(PHANDLE)evt);
#endif
}

int ls_event_reset(ls_handle evt)
{
#if LS_WINDOWS
	return !ResetEvent(*(PHANDLE)evt);
#endif
}
