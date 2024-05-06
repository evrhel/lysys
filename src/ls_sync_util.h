#ifndef _LS_SYNC_UTIL_H_
#define _LS_SYNC_UTIL_H_

#include <stdio.h>
#include <stdlib.h>

#include "ls_native.h"

#ifdef LS_WINDOWS
#define __LOCK_T CRITICAL_SECTION
#define __COND_T CONDITION_VARIABLE
#else
#define __LOCK_T pthread_mutex_t
#define __COND_T pthread_cond_t
#endif // LS_WINDOWS

//! \brief Lock providing mutal exclusion
typedef __LOCK_T ls_lock_t;

//! \brief Condition variable
typedef __COND_T ls_cond_t;

//! \brief Create a lock
//! 
//! always succeeds on Windows
//!
//! \param lock A lock
//!
//! \return 0 on success, -1 on failure
int lock_init(ls_lock_t *lock);

//! \brief Destroy a lock
//!
//! \param lock A lock
void lock_destroy(ls_lock_t *lock);

//! \brief Acquire a lock
//!
//! \param lock A ls_lock_t
void lock_lock(ls_lock_t *lock);

//! \brief Try to acquire a lock
//!
//! \param lock A lock
//!
//! \return 0 if the lock was acquired, 1 if it was already locked
int lock_trylock(ls_lock_t *lock);

//! \brief Release a lock
//!
//! \param lock A lock
void lock_unlock(ls_lock_t *lock);

//! \brief Initialize a condition variable
//! 
//! always succeeds on Windows
//!
//! \param cond The condition to initialize
//!
//! \return 0 on succes, -1 on failure
int cond_init(ls_cond_t *cond);

//! \brief Destroy a condition variable
//!
//! \param cond The condition to destroy
void cond_destroy(ls_cond_t *cond);

//! \brief Wait for the specified condition to be signaled or until a timeout occurs
//!
//! \param cond The condition to wait for
//! \param lock The lock to wait with
//! \param ms The maximum number of milliseconds to wait
//!
//! \return 0 on success, 1 on timeout
int cond_wait(ls_cond_t *RESTRICT cond, ls_lock_t *RESTRICT lock, unsigned long ms);

//! \brief Wake a signal waiting thread
//!
//! \param cond The condition to signal
void cond_signal(ls_cond_t *cond);

//! \brief Wake all waiting threads
//!
//! \param cond The condition to signal
void cond_broadcast(ls_cond_t *cond);

#endif // _LS_SYNC_UTIL_H_
