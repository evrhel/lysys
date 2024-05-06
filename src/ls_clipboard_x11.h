#ifndef _LS_CLIPBOARD_X11_H_
#define _LS_CLIPBOARD_X11_H_

#include "ls_native.h"
#include "ls_sync_util.h"
#include "ls_util.h"
#include "ls_buffer.h"

struct data;

//! \brief Clipboard instance
struct clipboard
{
    xcb_connection_t *conn; // Connection to X server
	xcb_window_t window; // Window to receive clipboard events

	ls_lock_t lock; // Lock
    ls_cond_t cond; // Condition variable
	pthread_t thread; // Event loop worker

    map_t *formats; // const char * -> xcb_atom_t (custom formats)
    map_t *data; // xcb_atom_t -> struct data * (clipboard data)

    xcb_atom_t target_atom;

    int async_error; // error in event loop

    ls_buffer_t reply_buffer;
    int reply_incr_process;
    int reply_incr_received;

    int (*on_reply)(struct clipboard *self);
    int callback_result;
    int callback_errno;
    
    // for read_clipboard_data
    struct
    {
        void *buf; // inout
        size_t cb; // inout
    } read_clipboard_data;
};

//! \brief Register a clipboard format
//!
//! \param self Clipboard instance
//! \param name Format name
//! 
//! \return Format identifier, or -1 on failure
intptr_t clipboard_register_format(struct clipboard *self, const char *name);
// Implementation note: lock should not be held

//! \brief Set clipboard data
//!
//! \param self Clipboard instance
//! \param fmt Format identifier
//! \param data Data
//! \param cb Data size
//!
//! \return 0 on success, -1 on failure
int clipboard_set_data(struct clipboard *self, intptr_t fmt, const void *data, size_t cb);
// Implementation note: lock should not be held

//! \brief Clear clipboard data
//!
//! \param self Clipboard instance
//!
//! \return 0 on success, -1 on failure
int clipboard_clear_data(struct clipboard *self);
// Implementation note: lock should not be held

//! \brief Get clipboard data size
//!
//! \param self Clipboard instance
//! \param fmt Format identifier
//! \param data Data
//! \param cb Data size
//!
//! \return Data size, 0 if not available in requested format, or -1
//! on failure
size_t clipboard_get_data(struct clipboard *self, intptr_t fmt, void *data, size_t cb);

//! \brief Initialize a clipboard instance
//!
//! \param self Clipboard instance
//!
//! \return 0 on success, -1 on error
int clipboard_init(struct clipboard *self);

//! \brief Release a clipboard instance
//!
//! \param self Clipboard instance
void clipboard_release(struct clipboard *self);

#endif // _LS_CLIPBOARD_X11_H_
