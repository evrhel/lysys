#ifndef _LS_THREAD_H_
#define _LS_THREAD_H_

#include "ls_defs.h"

#define LS_THREAD_PRIORITY_LOWEST -2
#define LS_THREAD_PRIORITY_BELOW_NORMAL -1
#define LS_THREAD_PRIORITY_NORMAL 0
#define LS_THREAD_PRIORITY_ABOVE_NORMAL 1
#define LS_THREAD_PRIORITY_HIGHEST 2

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

//! \brief Set the scheduling priority of a thread.
//! 
//! \param th The thread handle.
//! \param priority The new priority of the thread.
//! 
//! \return 0 if successful, or -1 if an error occurred.
int ls_thread_set_priority(ls_handle th, int priority);

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

//! \brief Convert the calling thread to a fiber.
//! 
//! Converts the calling thread to a fiber. The thread can now be
//! used to create fibers and switch between them.
//! 
//! \param up User data to associate with the fiber. This data can be
//! retrieved with ls_fiber_get_data().
//! 
//! \return 0 if successful, or -1 if an error occurred.
int ls_convert_to_fiber(void *up);

//! \brief Convert the calling fiber to a thread.
//! 
//! Converts the calling fiber to a thread. The thread cannot do
//! any operations that are specific to fibers, including closing
//! the handle. Close handles before converting to a thread.
//! 
//! \return 0 if successful, or -1 if an error occurred.
int ls_convert_to_thread(void);

//! \brief Create a new fiber.
//! 
//! Fibers may only be created by a fiber. Use ls_convert_to_fiber() to
//! convert the calling thread to a fiber.
//! 
//! If a fiber was not created with ls_convert_to_fiber() or
//! ls_fiber_create(), use of the fiber functions within that fiber
//! will result in undefined behavior.
//! 
//! \param func The function to run in the new fiber.
//! \param up User data to pass to the fiber function.
//! 
//! \return A handle to the new fiber, or NULL if an error occurred.
ls_handle ls_fiber_create(ls_thread_func_t func, void *up);

//! \brief Switch to the specified fiber.
//! 
//! Do not switch to a already running fiber. If the current thread
//! is not a fiber, the function returns immediately.
//! 
//! \param fiber The fiber to switch to.
void ls_fiber_switch(ls_handle fiber);

//! \brief Switch to the main fiber on the current thread.
void ls_fiber_sched(void);

//! \brief Retrieve the handle of the calling fiber.
//! 
//! The returned handle is a psuedo-handle to the calling fiber and
//! will always refer to the calling fiber, regardless of the thread
//! or fiber that receives the handle.
//! 
//! \return A handle to the calling fiber.
ls_handle ls_fiber_self(void);

//! \brief Get the user data associated with a fiber.
//! 
//! \param fiber The fiber handle.
//! 
//! \return The user data associated with the fiber. If the current
//! thread is not a fiber, the function returns NULL and sets ls_errno.
void *ls_fiber_get_data(ls_handle fiber);

//! \brief Exits the current fiber.
//! 
//! If the caller is a fiber, the thread that created the fiber will be
//! scheduled. If the caller is a thread or is the creator of the fiber,
//! the thread will exit. Regardless, the call will not return.
//! 
//! \param code The exit code.
LS_NORETURN void ls_fiber_exit(int code);

#endif // _LS_THREAD_H_
