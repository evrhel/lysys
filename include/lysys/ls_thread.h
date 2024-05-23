#ifndef _LS_THREAD_H_
#define _LS_THREAD_H_

#include "ls_defs.h"

typedef int(*ls_thread_func_t)(void *up);

//! \brief Create a thread.
//!
//! Allocates a new thread and starts it running the specified function.
//!
//! \param func The function to run in the new thread.
//! \param up User data to pass to the thread function.
//!
//! \return A handle to the new thread, or NULL if an error occurred.
ls_handle ls_thread_create(ls_thread_func_t func, void *up);

//! \brief Query thread ID by handle.
//!
//! \param th The thread handle.
//!
//! \return The thread ID.
unsigned long ls_thread_id(ls_handle th);

//! \brief Query the ID of the calling thread.
//!
//! \return The thread ID.
unsigned long ls_thread_id_self(void);

//! \brief Get a pseudo-handle to the calling thread.
//! 
//! Returns a pseudo-handle to the calling thread. The handle does not
//! need to be closed. The handle always refers to the calling thread.
//! If the the handle is passed accross a thread boundary, the handle
//! will refer to the thread that received the handle.
//! 
//! \return A pseudo-handle to the calling thread.
ls_handle ls_thread_self(void);

//! \brief Yield the calling thread.
//!
//! Causes the calling thread to yield the processor to another thread.
void ls_yield(void);

//! \brief Allocate a new Thread Local Storage (TLS) key.
//!
//! TLS keys are used to store thread-specific data. Each thread has its
//! own copy of the data associated with the key. Keys cannot be shared
//! between processes. Release this key with ls_close() when it is no
//! longer needed.
//!
//! \return A handle to the TLS key or NULL if an error occurred.
ls_handle ls_tls_create(void);

//! \brief Set the value associated with a TLS key.
//!
//! \param tls The TLS key.
//! \param value The value to associate with the key.
//!
//! \return 0 if successful, or -1 if an error occurred.
int ls_tls_set(ls_handle tlsh, void *value);

//! \brief Get the value associated with a TLS key.
//!
//! \param tls The TLS key.
//!
//! \return The value associated with the key, or NULL if an error
//! occurred. Note that NULL is also a valid value.
void *ls_tls_get(ls_handle tlsh);

typedef void(*ls_apc_routine)(void *arg);

//! \brief Queue an APC to a thread
//! 
//! \param routine The APC routine to be executed
//! \param thread_id The thread ID of the target thread. Use 0 to
//! target the current thread.
//! \param arg The argument to be passed to the APC routine
//! 
//! \return 0 on success, -1 on failure
int ls_queue_apc(ls_apc_routine routine, unsigned long thread_id, void *arg);

//! \brief Set the current thread to alertable state
//! 
//! When a thread is in an alertable state, it is allowed to execute
//! user-mode APCs or I/O completion routines.
void ls_set_alertable(void);

#endif // _LS_THREAD_H_
