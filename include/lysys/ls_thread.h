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
unsigned long ls_thread_self(void);

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

#endif // _LS_THREAD_H_
