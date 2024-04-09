#ifndef _LS_FSEVENT_H_
#define _LS_FSEVENT_H_

#include "ls_defs.h"

#define LS_WATCH_ADD 1 //!< File added
#define LS_WATCH_REMOVE 2 //!< File removed
#define LS_WATCH_MODIFY 3 //!< File modified
#define LS_WATCH_RENAME 4 //!< File renamed

struct ls_watch_event
{
	int action;			//!< Action
	const char *source;	//!< Source path
	const char *target;	//!< Target path (if action is rename, otherwise NULL)
};

//! \brief Monitor file system events in a directory.
//!
//! \details Creates a new file system event monitor for the specified
//! directory. Events are handled asynchronously and can be retrieved using
//! ls_watch_get_result(). The returned handle is waitable, and can be used
//! with ls_wait() to wait for events to occur.
//!
//! \param dir The directory to monitor.
//! \param recursive Whether to monitor the directory recursively.
//!
//! \return A handle to the file system event monitor, or NULL if an error
//! occurred.
ls_handle ls_watch_dir(const char *dir, int recursive);

//! \brief Retrieve the next file system event.
//!
//! \details Retrieves the next file system event from the specified file
//! system event monitor. If no events are available, the function returns
//! immediately and the contents of the event structure are undefined.
//!
//! \param watch The file system event monitor.
//! \param event The event structure to fill with the event data.
//!
//! \return 0 if an event was retrieved, 1 if no events are available, or
//! -1 if an error occurred.
int ls_watch_get_result(ls_handle watch, struct ls_watch_event *event);

#endif // _LS_FSEVENT_H_
