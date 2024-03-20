#include <lysys/ls_event.h>

#include <lysys/ls_core.h>

#include "ls_native.h"
#include "ls_handle.h"

#if LS_WINDOWS

static int ls_event_wait(HANDLE hEvent)
{
	DWORD dwResult;

	dwResult = WaitForSingleObject(hEvent, INFINITE);
	if (dwResult == WAIT_OBJECT_0) return 0;

	return -1;
}
#endif

static struct ls_class EventClass = {
	.type = LS_EVENT,
#if LS_WINDOWS
	.cb = sizeof(HANDLE),
	.dtor = (ls_dtor_t)&CloseHandle,
#endif
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
	return WaitForSingleObject(evt, 0) == WAIT_OBJECT_0;
#endif
}

int ls_event_set(ls_handle evt)
{
#if LS_WINDOWS
	return !SetEvent(evt);
#endif
}

int ls_event_reset(ls_handle evt)
{
#if LS_WINDOWS
	return !ResetEvent(evt);
#endif
}
