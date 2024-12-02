#ifndef _LS_FSEVENT_H_
#define _LS_FSEVENT_H_

#include "ls_defs.h"

#define LS_WATCH_ERROR -1 //!< An error occurred
#define LS_WATCH_NONE 0 //!< No event
#define LS_WATCH_ADD 1 //!< File added
#define LS_WATCH_REMOVE 2 //!< File removed
#define LS_WATCH_MODIFY 3 //!< File modified
#define LS_WATCH_RENAME 4 //!< File renamed

#define LS_WATCH_FLAG_NONE 0x0
#define LS_WATCH_FLAG_RECURSIVE 0x1 //!< Monitor subdirectories
#define LS_WATHC_FLAG_ONLY_EVENTS 0x2 //!< Only monitor events

struct ls_watch_event
{
    intptr_t type; //!< type of event
    size_t old_name; //!< offset into filename where the old file name is stored
    char filename[1];
};

//! \brief Monitor file system events in a directory.
//!
//! Creates a new file system event monitor for the  specified
//! directory. Events are handled asynchronously and can be retrieved
//! using ls_watch_get_result(). The returned handle is waitable, and
//! can be used  with ls_wait() to wait for events to occur.
//! 
//! Avoid monitoring directories with a large number of files or
//! directories with large subtrees, as this can cause the monitor to
//! consume a significant amount of memory, CPU resources, and may
//! bottleneck I/O operations.
//!
//! \param dir The directory to monitor.
//! \param flags Flags to control the behavior of the monitor.
//!
//! \return A handle to the file system event monitor, or NULL if an
//! error occurred.
ls_handle ls_watch_dir(const char *dir, int flags);

//! \brief Retrieve the next file system event.
//!
//! Dequeues a file system event from the monitor and stores a
//! description in the given structure. The event parameter has
//! varying length and should be dynamically allocated. Pass NULL
//! to determine the number of bytes to allocate. If event is NULL,
//! cb must be 0.
//!
//! \param watch The file system event monitor.
//! \param event Event structure to store the event.
//! \param cb The number of bytes pointed to by event.
//!
//! \return Returns the required number of bytes to store the event,
//! or -1 on error. If 0 is returned, no events are available.
size_t ls_watch_get_result(ls_handle watch, struct ls_watch_event *event, size_t cb);

#endif // _LS_FSEVENT_H_
