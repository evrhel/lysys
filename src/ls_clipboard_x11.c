// This implementation is based off of the following:
// https://github.com/dacap/clip/blob/main/clip_x11.cpp
// The original is licensed under the MIT License

#include "ls_clipboard_x11.h"

#include <string.h>

#include <lysys/ls_core.h>
#include <lysys/ls_clipboard.h>

#include "ls_native.h"
#include "ls_sync_util.h"

#define ATOM_ATOM "ATOM"
#define ATOM_INCR "INCR"
#define ATOM_TARGETS "TARGETS"
#define ATOM_CLIPBOARD "CLIPBOARD"
#define ATOM_ATOM_PAIR "ATOM_PAIR"
#define ATOM_SAVE_TARGETS "SAVE_TARGETS"
#define ATOM_MULTIPLE "MULTIPLE"
#define ATOM_CLIPBOARD_MANAGER "CLIPBOARD_MANAGER"

static const char *TEXT_FORMATS[] = {
    "UTF8_STRING",
    "text/plain;charset=utf-8",
    "text/plain;charset=UTF-8",
    "GTK_TEXT_BUFFER_CONTENTS",
    "STRING",
    "TEXT",
    "text/plain"
};

#define NTEXT_FORMATS (sizeof(TEXT_FORMATS)/sizeof(TEXT_FORMATS[0]))

#define TIMEOUT 100 // 100ms

struct data
{
    xcb_atom_t type; // data type
    size_t cb; // data size
    uint8_t *data; // data
};

//! \brief Free data
//!
//! \param self A data instance
static void data_free(struct data *self)
{
    if (self)
    {
        if (self->data)
            ls_free(self->data);
        ls_free(self);
    }
}

//! \brief Duplicate data
//!
//! \param self A data instance
//!
//! \return A copy of the data instance
static void *data_dup(struct data *self)
{
    struct data *copy;

    if (!self)
        return NULL;

    copy = ls_malloc(sizeof(struct data));
    if (!copy)
        return NULL;

    copy->type = self->type;
    copy->cb = self->cb;

    if (self->data)
    {
        copy->data = ls_malloc(self->cb);
        if (!copy->data)
        {
            ls_free(copy);
            return NULL;
        }

        memcpy(copy->data, self->data, self->cb);
    }
    else
        copy->data = NULL;

    return copy;
}

//! \brief Get an atom by name, creating it if necessary
//!
//! \param self Clipboard instance
//! \param name Atom name
//!
//! \return Atom ID, or XCB_ATOM_NONE on error
static xcb_atom_t get_atom(struct clipboard *self, const char *name)
{
    xcb_atom_t atom;
    xcb_intern_atom_cookie_t cookie;
    xcb_intern_atom_reply_t *reply;

    cookie = xcb_intern_atom(self->conn, 0, strlen(name), name);
    reply = xcb_intern_atom_reply(self->conn, cookie, NULL);

    if (!reply)
    {
        ls_set_errno(LS_IO_ERROR);
        return XCB_ATOM_NONE;
    }

    atom = reply->atom;
    free(reply);
    return atom;
}

//! \brief Get an atom for a format
//!
//! \param self Clipboard instance
//! \param fmt Format identifier
//!
//! \return Atom ID, or XCB_ATOM_NONE on error
static xcb_atom_t get_atom_for_format(struct clipboard *self,
    intptr_t fmt)
{
    entry_t *entry;

    if (fmt == LS_CF_TEXT)
        return get_atom(self, TEXT_FORMATS[0]);

    entry = (entry_t *)fmt;
    return entry->value.u32;
}

//! \brief Set ourselves as the owner of the clipboard
//!
//! \param self Clipboard instance
//!
//! \return 0 on success, -1 on error
static int set_owner(struct clipboard *self)
{
    xcb_void_cookie_t cookie;
    xcb_generic_error_t *error;

    cookie = xcb_set_selection_owner_checked(self->conn,
        self->window, get_atom(self, ATOM_CLIPBOARD),
        XCB_CURRENT_TIME);

    error = xcb_request_check(self->conn, cookie);
    if (error)
    {
        free(error);
        return ls_set_errno(LS_IO_ERROR);
    }

    return 0;
}

//! \brief Get the owner of the clipboard
//!
//! \param self Clipboard instance
//!
//! \return Owner window, or 0 if no owner
static xcb_window_t get_owner(struct clipboard *self)
{
    xcb_window_t window;
    xcb_get_selection_owner_cookie_t cookie;
    xcb_get_selection_owner_reply_t *reply;

    cookie = xcb_get_selection_owner(self->conn,
        get_atom(self, ATOM_CLIPBOARD));
    reply = xcb_get_selection_owner_reply(self->conn, cookie, NULL);

    if (!reply)
    {
        ls_set_errno(LS_IO_ERROR);
        return 0;
    }

    window = reply->owner;
    free(reply);
    return window;
}

//! \brief Read data from a data instance
//!
//! \param d Data instance
//! \param buf Buffer to receive data
//! \param cb Size of the buffer
//!
//! \return Number of bytes copied to the buffer or -1 on error
static size_t read_data(struct data *d, void *buf, size_t cb)
{
    if (!d)
        return ls_set_errno(LS_INVALID_ARGUMENT);

    if (!buf != !cb)
        return ls_set_errno(LS_INVALID_ARGUMENT);

    if (!buf)
        return d->cb;

    if (cb < d->cb)
        return ls_set_errno(LS_BUFFER_TOO_SMALL);

    memcpy(buf, d->data, d->cb);
    return d->cb;
}

//! \brief Get data from the owner of the clipboard
//!
//! \param self Clipboard instance
//! \param start Start of the atom list
//! \param end End of the atom list
//! \param on_reply Callback function
//! \param selection Selection atom
//!
//! \return 0 on success, -1 on error
static int get_data_from_owner(struct clipboard *self,
    xcb_atom_t *start, xcb_atom_t *end,
    int (*on_reply)(struct clipboard *), xcb_atom_t selection)
{
    xcb_window_t owner;
    xcb_atom_t *atom;
    int rc;

    if (!selection)
        selection = get_atom(self, ATOM_CLIPBOARD);

    self->on_reply = on_reply;
    self->callback_result = -1;
    self->callback_errno = LS_BUSY;

    if (self->window != get_owner(self))
        ls_map_clear(self->data);

    // request data for each atom
    for (atom = start; atom < end; atom++)
    {
        xcb_convert_selection(self->conn, self->window, selection,
            *atom, get_atom(self, ATOM_CLIPBOARD), XCB_CURRENT_TIME);
        xcb_flush(self->conn);

        do
        {
            self->reply_incr_received = 0;

            rc = cond_wait(&self->cond, &self->lock, TIMEOUT);
            if (rc == 0)
            {
                // check for event loop failure
                if (self->async_error)
                {
                    _ls_errno = self->async_error;
                    return -1;
                }

                _ls_errno = self->callback_errno;
                return self->callback_result;
            }
        } while (self->reply_incr_received);
    }

    // not available
    self->on_reply = NULL;
    self->callback_errno = 0;
    self->callback_result = 0;
    return ls_set_errno(LS_NOT_FOUND);
}

//! \brief Callback function for read_clipboard_data
//!
//! \param self Clipboard instance
//!
//! \return 0 on success, -1 on error
static int read_clipboard_data_callback(struct clipboard *self)
{
    void *buf;
    size_t cb;
    size_t avail;

    buf = self->read_clipboard_data.buf;
    cb = self->read_clipboard_data.cb;

    if (!buf != !cb)
        return ls_set_errno(LS_INVALID_ARGUMENT);

    avail = self->reply_buffer.pos - self->reply_buffer.data;
    self->read_clipboard_data.cb = avail;
    
    if (!buf)
        return 0;

    if (cb < avail)
        return ls_set_errno(LS_BUFFER_TOO_SMALL);

    memcpy(buf, self->reply_buffer.data, avail);
    
    return 0;
}

//! \brief Retrieve custom data from the clipboard
//!
//! \param self Clipboard instance
//! \param atom Atom ID
//! \param data Buffer to receive data
//! \param cb Size of the buffer
//!
//! \return Number of bytes copied to the buffer, or 0 if no data
//! is available, or -1 on error
static size_t read_clipboard_data(struct clipboard *self,
    xcb_atom_t atom, void *data, size_t cb)
{
    entry_t *ent;
    int rc;

    if (!data != !cb)
        return ls_set_errno(LS_INVALID_ARGUMENT);

    if (get_owner(self) == self->window)
    {
        ent = ls_map_find(self->data, ANY_U32(atom));
        if (!ent)
            return 0; // not available in requested format

        return read_data(ent->value.ptr, data, cb);
    }

    self->read_clipboard_data.buf = data;
    self->read_clipboard_data.cb = cb;

    rc = get_data_from_owner(self, &atom,
        &atom + 1, &read_clipboard_data_callback, 0);
    if (rc == -1)
        return -1;

    return self->read_clipboard_data.cb;
}

//! \brief Consume a get property reply, and delete if requested
//!
//! \param self Clipboard instance
//! \param window Requestor window
//! \param property Property atom
//! \param atom Target atom
//! \param remove Whether to remove the property
//!
//! \return Reply, or NULL on error
static xcb_get_property_reply_t *consume_get_property_reply(
    struct clipboard *self, xcb_window_t window, xcb_atom_t property,
    xcb_atom_t atom, int remove)
{
    xcb_get_property_cookie_t cookie;
    xcb_generic_error_t *error = NULL;
    xcb_get_property_reply_t *reply;

    cookie = xcb_get_property(self->conn, remove, window,
        property, atom, 0, INT32_MAX / 4);

    reply = xcb_get_property_reply(self->conn, cookie, &error);
    if (error)
    {
        ls_set_errno(LS_IO_ERROR);
        free(error);
        return NULL;
    }

    return reply;
}

//! \brief Set the clipboard content for a requestor
//!
//! \param self Clipboard instance
//! \param requestor Requestor window
//! \param property Property atom
//! \param target Target atom
//!
//! \return 0 on success, -1 on error (unsupported format)
static int set_requestor_clipboard_content(struct clipboard *self,
    xcb_atom_t requestor, xcb_atom_t property, xcb_atom_t target)
{
    entry_t *ent;
    struct data *d;

    ent = ls_map_find(self->data, ANY_U32(target));
    if (!ent)
        return ls_set_errno(LS_NOT_SUPPORTED); // unsupported format
    
    d = ent->value.ptr;

    xcb_change_property(self->conn, XCB_PROP_MODE_REPLACE,
        requestor, property, target, 8, d->cb, d->data);

    return 0;
}

//! \brief Write reply data to the reply buffer
//!
//! \param self Clipboard instance
//! \param reply Get property reply
//!
//! \return 0 on success, -1 on error
static int write_reply_data(struct clipboard *self, xcb_get_property_reply_t *reply)
{
    size_t n;
    void *data;

    n = xcb_get_property_value_length(reply);
    data = xcb_get_property_value(reply);
    
    return ls_buffer_write(&self->reply_buffer, data, n);
}

//! \brief Synchronously call the callback function and signal the condition variable
//!
//! \param self Clipboard instance
//!
//! \return 0 on success, -1 on error (same as callback_result)
static int call_callback(struct clipboard *self)
{
    int rc = 0;

    _ls_errno = 0;

    if (self->on_reply)
        rc = self->on_reply(self);
    self->callback_result = rc;
    self->callback_errno = _ls_errno;

    cond_signal(&self->cond);
    ls_buffer_clear(&self->reply_buffer);

    return rc;
}

//
/////////////////////////////////////////////////////////////////////
// X event handlers
//

static int handle_selection_clear(struct clipboard *self,
    xcb_selection_clear_event_t *evt)
{
    lock_lock(&self->lock);

    // clear clipboard if selected
    if (evt->selection == get_atom(self, ATOM_CLIPBOARD))
        ls_map_clear(self->data);

    lock_unlock(&self->lock);
    return 0;
}

static int handle_selection_request(struct clipboard *self,
    xcb_selection_request_event_t *evt)
{
    xcb_selection_notify_event_t notify;
    xcb_get_property_reply_t *reply;
    xcb_atom_t *atoms = NULL;
    size_t natoms;
    entry_t *ent;
    struct data *d;
    size_t i;
    xcb_atom_t *patom, *pend;
    xcb_atom_t target, property;
    int rc;

    lock_lock(&self->lock);

    if (evt->target == get_atom(self, ATOM_TARGETS))
    {
        // set the requestor's property to the list of supported formats

        natoms = 3 + self->data->size;
        atoms = ls_malloc(sizeof(xcb_atom_t) * natoms);
        if (!atoms)
        {
            lock_unlock(&self->lock);
            return -1;
        }

        atoms[0] = get_atom(self, ATOM_TARGETS);
        atoms[1] = get_atom(self, ATOM_SAVE_TARGETS);
        atoms[2] = get_atom(self, ATOM_MULTIPLE);
        
        ent = self->data->entries;
        for (i = 3; ent; ent = ent->next, i++)
            atoms[i] = ent->key.u32;

        xcb_change_property(self->conn, XCB_PROP_MODE_REPLACE,
            evt->requestor, evt->property, get_atom(self, ATOM_ATOM),
            8*sizeof(xcb_atom_t), natoms, atoms);

        // atoms freed after xcb_flush
    }
    else if (evt->target == get_atom(self, ATOM_SAVE_TARGETS))
    {
        // ignore
    }
    else if (evt->target == get_atom(self, ATOM_MULTIPLE))
    {
        reply = consume_get_property_reply(self, evt->requestor,
            evt->property, get_atom(self, ATOM_ATOM_PAIR), 0);
        if (!reply)
        {
            lock_unlock(&self->lock);
            return -1;
        }

        patom = xcb_get_property_value(reply);
        pend = patom + (xcb_get_property_value_length(reply) / sizeof(xcb_atom_t));

        while (patom < pend)
        {
            target = *patom++;
            property = *patom++;
            
            // ignore unsupported formats
            rc = set_requestor_clipboard_content(self, evt->requestor, property, target);
            if (rc == 0)
            {
                xcb_change_property(self->conn, XCB_PROP_MODE_REPLACE,
                    evt->requestor, evt->property, XCB_ATOM_NONE, 0, 0, NULL);
            }
        }

        free(reply);
    }
    else
    {
        // set the requestor's property to the data, if available
        (void)set_requestor_clipboard_content(self, evt->requestor, evt->property, evt->target);
    }

    // notify the requestor

    notify.response_type = XCB_SELECTION_NOTIFY;
    notify.pad0 = 0;
    notify.sequence = 0;
    notify.time = evt->time;
    notify.requestor = evt->requestor;
    notify.selection = evt->selection;
    notify.target = evt->target;
    notify.property = evt->property;

    xcb_send_event(self->conn, 0, evt->requestor,
        XCB_EVENT_MASK_NO_EVENT, (const char *)&notify);
    xcb_flush(self->conn);

    lock_unlock(&self->lock);

    if (atoms)
        ls_free(atoms);

    return 0;
}

static int handle_selection_notify(struct clipboard *self,
    xcb_selection_notify_event_t *evt)
{
    xcb_get_property_reply_t *reply;
    uint32_t n;
    size_t count;
    int rc;

    lock_lock(&self->lock);

    if (evt->target == get_atom(self, ATOM_TARGETS))
        self->target_atom = get_atom(self, ATOM_ATOM);
    else
        self->target_atom = evt->target;

    // get the data 
    reply = consume_get_property_reply(self, evt->requestor,
        evt->property, self->target_atom, 1);
    if (!reply)
    {
        lock_unlock(&self->lock);
        return 0; // ignore
    }

    if (reply->type == get_atom(self, ATOM_INCR))
    {
        // data is too large to fit in a single property, will be recieved
        // in multiple INCR properties

        free(reply);

        reply = consume_get_property_reply(self, evt->requestor,
            evt->property, get_atom(self, ATOM_INCR), 1);
        
        if (reply)
        {
            if (xcb_get_property_value_length(reply) == 4)
            {
                // reserve space for the data
                n = *(uint32_t *)xcb_get_property_value(reply);
                
                ls_buffer_clear(&self->reply_buffer);
                rc = ls_buffer_reserve(&self->reply_buffer, n);
                if (rc == -1)
                {
                    free(reply);
                    lock_unlock(&self->lock);
                    return -1; // out of memory
                }

                self->reply_incr_process = 1;
                self->reply_incr_received = 1;
            }

            free(reply);
        }
    }
    else
    {
        // data is in the property
        ls_buffer_clear(&self->reply_buffer);

        rc = write_reply_data(self, reply);
        if (rc == -1)
        {
            free(reply);
            lock_unlock(&self->lock);
            return -1;
        }

        rc = call_callback(self);
        if (rc == -1)
        {
            free(reply);
            lock_unlock(&self->lock);
            return -1;
        }


        free(reply);
    }

    lock_unlock(&self->lock);
    return 0;
}

static int handle_property_notify(struct clipboard *self,
    xcb_property_notify_event_t *evt)
{
    xcb_get_property_reply_t *reply;
    int rc;

    if (self->reply_incr_process &&
        evt->state == XCB_PROPERTY_NEW_VALUE &&
        evt->atom == get_atom(self, ATOM_CLIPBOARD))
    {
        reply = consume_get_property_reply(self, evt->window,
            evt->atom, self->target_atom, 1);
        if (!reply)
            return -1;

        self->reply_incr_received = 1;

        if (xcb_get_property_value_length(reply) > 0)
        {
            rc = write_reply_data(self, reply);
            if (rc == -1)
            {
                free(reply);
                return -1;
            }
        }
        else
        {
            rc = call_callback(self);
            if (rc == -1)
            {
                free(reply);
                return -1;
            }

            self->reply_incr_process = 0;
        }

        free(reply);
    }

    return 0;
}

//! \brief Worker thread for clipboard events
//!
//! \param self Clipboard instance
//!
//! \return Unused
static void *xcb_event_loop(struct clipboard *self)
{
	xcb_generic_event_t *evt;
    int rc;

	while ((evt = xcb_wait_for_event(self->conn)))
	{
		switch (evt->response_type & ~0x80)
		{
		default:
			break; // ignore
		case XCB_DESTROY_NOTIFY:
            rc = ls_set_errno(LS_IO_ERROR);
            break;
		case XCB_SELECTION_CLEAR:
            rc = handle_selection_clear(self,
                (xcb_selection_clear_event_t *)evt);
			break;
		case XCB_SELECTION_REQUEST:
            rc = handle_selection_request(self,
                (xcb_selection_request_event_t *)evt);
			break;
		case XCB_SELECTION_NOTIFY:
            rc = handle_selection_notify(self,
                (xcb_selection_notify_event_t *)evt);
			break;
		case XCB_PROPERTY_NOTIFY:
            rc = handle_property_notify(self,
                (xcb_property_notify_event_t *)evt);
			break;
		}

		free(evt);
	
        if (rc == -1)
        {
            lock_lock(&self->lock);
            self->async_error = _ls_errno;
            cond_broadcast(&self->cond);
            lock_unlock(&self->lock);
            break;
        }
    }

	return NULL;
}

//
/////////////////////////////////////////////////////////////////////
// Public interface
//

intptr_t clipboard_register_format(struct clipboard *self,
    const char *name)
{
    xcb_atom_t atom;
    entry_t *ent;
    int rc;

    if (!name)
        return ls_set_errno(LS_INVALID_ARGUMENT);

    lock_lock(&self->lock);

    if (self->async_error)
    {
        ls_set_errno(self->async_error);
        lock_unlock(&self->lock);
        return -1;
    }

    // check if the format is already registered
    ent = ls_map_find(self->formats, ANY_CPTR(name));
    if (ent)
    {
        lock_unlock(&self->lock);
        return (intptr_t)ent;
    }

    // create an atom for the format
    atom = get_atom(self, name);
    if (atom == XCB_ATOM_NONE)
    {
        lock_unlock(&self->lock);
        return -1;
    }

    // store the atom
    ent = ls_map_insert(self->formats, ANY_CPTR(name), ANY_U32(atom));
    if (!ent)
    {
        lock_unlock(&self->lock);
        return -1;
    }

    lock_unlock(&self->lock);
    return (intptr_t)ent;
}

int clipboard_set_data(struct clipboard *self, intptr_t fmt,
    const void *data, size_t cb)
{
    xcb_atom_t atom;
    struct data *d;
    int rc;

    lock_lock(&self->lock);

    if (self->async_error)
    {
        ls_set_errno(self->async_error);
        lock_unlock(&self->lock);
        return -1;
    }

    // set ourselves as the owner
    rc = set_owner(self);
    if (rc == -1)
    {
        lock_unlock(&self->lock);
        return -1;
    }

    // get the atom for the requested format
    atom = get_atom_for_format(self, fmt);
    if (atom == XCB_ATOM_NONE)
    {
        lock_unlock(&self->lock);
        return -1;
    }

    // allocate a new data instance
    d = ls_malloc(sizeof(struct data));
    if (!d)
    {
        lock_unlock(&self->lock);
        return -1;
    }

    d->type = atom;
    d->data = ls_malloc(cb);
    if (!d->data)
    {
        lock_unlock(&self->lock);
        ls_free(d);
        return -1;
    }

    memcpy(d->data, data, cb);
    d->cb = cb;

    // store the data
    ls_map_insert(self->data, ANY_U32(atom), ANY_PTR(d));

    lock_unlock(&self->lock);
    return 0;
}

int clipboard_clear_data(struct clipboard *self)
{
    xcb_window_t owner;
    xcb_selection_clear_event_t event;

    lock_lock(&self->lock);

    if (self->async_error)
    {
        ls_set_errno(self->async_error);
        lock_unlock(&self->lock);
        return -1;
    }

    ls_map_clear(self->data);

    // we are already the owner
    if (set_owner(self) == 0)
    {
        lock_unlock(&self->lock);
        return 0;
    }

    // get the current owner
    owner = get_owner(self);
    if (owner == self->window)
    {
        lock_unlock(&self->lock);
        return 0;
    }

    event.response_type = XCB_SELECTION_CLEAR;
    event.pad0 = 0;
    event.sequence = 0;
    event.time = XCB_CURRENT_TIME;
    event.owner = owner;
    event.selection = get_atom(self, ATOM_CLIPBOARD);

    // request the current owner to clear the clipboard
    xcb_send_event(self->conn, 0, owner,
        XCB_EVENT_MASK_NO_EVENT, (const char *)&event);

    xcb_flush(self->conn);

    lock_unlock(&self->lock);
    return 0;
}

size_t clipboard_get_data(struct clipboard *self, intptr_t fmt,
    void *data, size_t cb)
{
    xcb_atom_t atom;
    size_t count;
    size_t i;

    if (!data != !cb)
        return ls_set_errno(LS_INVALID_ARGUMENT);

    lock_lock(&self->lock);

    if (self->async_error)
    {
        ls_set_errno(self->async_error);
        lock_unlock(&self->lock);
        return -1;
    }

    if (fmt == LS_CF_TEXT)
    {
        for (i = 0; i < NTEXT_FORMATS; i++)
        {
            atom = get_atom(self, TEXT_FORMATS[i]);
            if (atom == XCB_ATOM_NONE)
                continue;

            count = read_clipboard_data(self, atom, data, cb);
            if (count != 0)
            {
                lock_unlock(&self->lock);
                return count;
            }
        }

        lock_unlock(&self->lock);
        return 0;
    }

    atom = get_atom_for_format(self, fmt);
    if (atom == XCB_ATOM_NONE)
    {
        lock_unlock(&self->lock);
        return -1;
    }

    count = read_clipboard_data(self, atom, data, cb);

    lock_unlock(&self->lock);
    return count;
}

int clipboard_init(struct clipboard *self)
{
    const xcb_setup_t *setup;
	xcb_screen_t *screen;
	uint32_t event_mask;
	int rc;

    ls_zero_memory(self, sizeof(struct clipboard));

	if (lock_init(&self->lock) != 0)
        return -1;

    self->formats = ls_map_create((map_cmp_t)&strcmp,
        (map_free_t)&ls_free, (map_dup_t)&ls_strdup, NULL, NULL);
    if (!self->formats)
    {
        rc = _ls_errno;
        clipboard_release(self);
        return ls_set_errno(rc);
    }

    self->data = ls_map_create(NULL, NULL, NULL,
        (map_free_t)&data_free, (map_dup_t)&data_dup);
    if (!self->data)
    {
        rc = _ls_errno;
        clipboard_release(self);
        return ls_set_errno(rc);
    }

    // connect to X server
    self->conn = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(self->conn))
    {
        clipboard_release(self);
        return ls_set_errno(LS_IO_ERROR);
    }

    setup = xcb_get_setup(self->conn);
    if (!setup)
    {
        clipboard_release(self);
        return ls_set_errno(LS_IO_ERROR);
    }

    screen = xcb_setup_roots_iterator(setup).data;
    if (!screen)
    {
        clipboard_release(self);
        return ls_set_errno(LS_IO_ERROR);
    }

    event_mask = XCB_EVENT_MASK_PROPERTY_CHANGE
        | XCB_EVENT_MASK_STRUCTURE_NOTIFY;

    // create a window to receive clipboard events
    self->window = xcb_generate_id(self->conn);
    xcb_create_window(self->conn, 0, self->window, screen->root,
        0, 0, 1, 1, 0,  XCB_WINDOW_CLASS_INPUT_ONLY,
        screen->root_visual, XCB_CW_EVENT_MASK, &event_mask);

    // event loop on a separate thread
    rc = pthread_create(&self->thread, NULL,
        (void *(*)(void *))&xcb_event_loop, self);
    if (rc != 0)
    {
        clipboard_release(self);
        return ls_set_errno_errno(rc);
    }

    return 0;
}

void clipboard_release(struct clipboard *self)
{
    xcb_window_t owner;
    xcb_atom_t manager;
    xcb_atom_t atom;

    if (self->conn)
    {
        // store our data elsewhere
        owner = get_owner(self);
        if (self->data->size && self->window == owner)
        {
            manager = get_atom(self, ATOM_CLIPBOARD_MANAGER);
            if (manager)
            {
                lock_lock(&self->lock);
                atom = get_atom(self, ATOM_SAVE_TARGETS);
                (void)get_data_from_owner(self, &atom, &atom + 1,
                    NULL, manager);
                lock_unlock(&self->lock);
            }
        }

        if (self->window)
        {
            xcb_destroy_window(self->conn, self->window);
            xcb_flush(self->conn);
        }

        // wait for the event loop to exit
        if (self->thread)
            pthread_join(self->thread, NULL);

        xcb_disconnect(self->conn);
    }

    ls_map_destroy(self->data);
    ls_map_destroy(self->formats);

    ls_buffer_release(&self->reply_buffer);

    lock_destroy(&self->lock);

    ls_zero_memory(self, sizeof(struct clipboard));
}
