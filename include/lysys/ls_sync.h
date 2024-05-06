#ifndef _LS_SYNC_H_
#define _LS_SYNC_H_

#include "ls_defs.h"

//! \brief Create a lock.
//! 
//! \details The lock is not guaranteed to be reentrant and behavior
//! is undefined if a thread attempts to lock a lock it already holds.
//! 
//! \return A handle to the lock.
ls_handle ls_lock_create(void);

//! \brief Acquire a lock.
//! 
//! \details Acquires the lock. If the lock is already held by another
//! thread, the calling thread will block until the lock is available.
//! If the calling thread already holds the lock, behavior is undefined.
//! If the lock is NULL or invalid, behavior is undefined.
//!
//! \param [in] lock The lock to acquire.
void ls_lock(ls_handle lock);

//! \brief Attempt to acquire a lock.
//! 
//! \details Attempts to acquire the lock. If the lock is already held
//! by another thread, the calling thread will return immediately with
//! a failure. If the calling thread already holds the lock, behavior
//! is undefined. If the lock is NULL or invalid, behavior is undefined.
//! 
//! \param [in] lock The lock to attempt to acquire.
//! 
//! \return 0 if the lock was acquired, 1 if the lock is already held.
int ls_trylock(ls_handle lock);

//! \breif Release a lock.
//! 
//! \details Releases the lock. If the lock is not held by the calling
//! thread, behavior is undefined. If the lock is NULL or invalid,
//! behavior is undefined.
//! 
//! \param [in] lock The lock to release.
void ls_unlock(ls_handle lock);

//! \brief Create a condition variable.
//! 
//! \details The condition variable is used to block a thread until a
//! condition is met. The condition variable is associated with a lock
//! and the lock must be held by the calling thread when waiting on the
//! condition variable. To wait on the condition variable, the calling
//! thread must call ls_cond_wait() or ls_cond_timedwait(). Calling
//! ls_wait() or ls_timedwait() will return immediately with a failure.
ls_handle ls_cond_create(void);

//! \brief Wait on a condition variable.
//! 
//! \details The calling thread will block until the condition variable
//! is signaled. The lock must be held by the calling thread when waiting
//! on the condition variable. Upon return, the lock will be held by the
//! calling thread. If cond or lock is NULL or invalid, behavior is undefined.
//! 
//! \param [in] cond The condition variable to wait on.
//! \param [in] lock The lock to associate with the condition variable.
void ls_cond_wait(ls_handle cond, ls_handle lock);

//! \brief Wait on a condition variable with a timeout.
//! 
//! \details The calling thread will block until the condition variable
//! is signaled or the timeout expires. The lock must be held by the
//! calling thread when waiting on the condition variable. Upon return,
//! the lock will be held by the calling thread. If cond or lock is NULL
//! or invalid, behavior is undefined.
//! 
//! \param [in] cond The condition variable to wait on.
//! \param [in] lock The lock to associate with the condition variable.
//! \param [in] ms The timeout in milliseconds.
//! 
//! \return 0 if the condition variable was signaled or 1 if the timeout
//! expired.
int ls_cond_timedwait(ls_handle cond, ls_handle lock, unsigned long ms);

//! \brief Signal a condition variable.
//! 
//! \details Wakes up one thread waiting on the condition variable.
//!
//! \param [in] cond The condition variable to signal.
void ls_cond_signal(ls_handle cond);

//! \brief Broadcast a condition variable.
//! 
//! \details Wakes up all threads waiting on the condition variable.
//! 
//! \param [in] cond The condition variable to broadcast.
void ls_cond_broadcast(ls_handle cond);

#endif
