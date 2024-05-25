#ifndef _LS_HANDLE_H_
#define _LS_HANDLE_H_

#include <lysys/ls_defs.h>

#define LS_WAITABLE 0x1000
#define LS_IO_STREAM 0x2000

#define LS_FILE (1 | LS_IO_STREAM)
#define LS_FILEMAPPING 2
#define LS_DIR 3
#define LS_LOCK 4
#define LS_COND 5
#define LS_THREAD (6 | LS_WAITABLE)
#define LS_PROC (7 | LS_WAITABLE)
#define LS_EVENT (8 | LS_WAITABLE)
#define LS_WATCH (9 | LS_WAITABLE)
#define LS_TLS 10
#define LS_PERF_MONITOR 11
#define LS_SNAPSHOT 12
#define LS_AIO (13 | LS_WAITABLE)
#define LS_PIPE (14 | LS_IO_STREAM)

// handle is statically allocated, will never have memory deallocated
// or destructor called
#define LS_HANDLE_FLAG_STATIC 0x10000

/* Reserved psuedo-handles */

#define LS_PSUEDO_HANDLE_LOW ((ls_handle)0x00000000)
#define LS_PSUEDO_HANDLE_HIGH ((ls_handle)0x0000ffff)

#define LS_SELF ((ls_handle)0x0000fffe)

#define LS_IS_PSUEDO_HANDLE(h) ((h) >= LS_PSUEDO_HANDLE_LOW && (h) <= LS_PSUEDO_HANDLE_HIGH)

#define LS_HANDLE_DATA(hi) ((ls_handle)((hi) + 1))
#define LS_HANDLE_INFO(h) ((struct ls_handle_info *)(h)-1)
#define LS_HANDLE_CLASS(h) (LS_HANDLE_INFO(h)->clazz)

// handle info initializer for static handles
#define __hiinit(_clazz) \
	{ \
		.clazz = &_clazz, \
		.flags = LS_HANDLE_FLAG_STATIC, \
		.refcount = 1 \
	}

typedef void(*ls_dtor_t)(void *ptr);
typedef int(*ls_wait_t)(void *ptr, unsigned long ms);

//! \brief Class structure
struct ls_class
{
	int32_t type;	//!< Unique type identifier
	uint32_t cb;	//!< Size of class data
	ls_dtor_t dtor;	//!< Destructor. If NULL, no destructor is called.
	ls_wait_t wait;	//!< Wait. If type is waitable, this must not be NULL.
};

//! \brief Handle information.
struct ls_handle_info
{
	const struct ls_class *clazz;	//!< Class of the handle
	int flags;					//!< Flags of the handle
	uint32_t reserved;				//!< Reserved for future use
};

//! \brief Create handle from class.
//! 
//! Creates a new handle of the given class.
//! 
//! \param clazz The class of the handle. Must be valid
//! through the lifetime of the handle.
//! \param flags Flags for the handle. What these mean depends on the
//! class.
//! 
//! \return The new handle. This is a pointer to the handle data. The
//! data will be initialized to zero.
ls_handle ls_handle_create(const struct ls_class *clazz, int flags);

//! \brief Deallocate memory used by the handle.
//!
//! Deallocates memory that was allocated through
//! ls_handle_create. Does not call the destructor. Use
//! ls_close to call the destructor and deallocate memory.
//! This does not check the reference count of the handle,
//! meaning the memory will always be deallocated. Statically
//! allocated handles or psuedo-handles will be ignored.
//! 
//! Will not affect _ls_errno.
//! 
//! \param h The handle to deallocate.
void ls_handle_dealloc(ls_handle h);

#endif // _LS_HANDLE_H_
