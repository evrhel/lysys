#ifndef _LS_HANDLE_H_
#define _LS_HANDLE_H_

#include <lysys/ls_defs.h>

#define LS_WAITABLE 0x1000
#define LS_READABLE 0x2000
#define LS_WRITABLE 0x4000
#define LS_FLAG_MASK 0x0fff

#define LS_FILE (1 | LS_READABLE | LS_WRITABLE)
#define LS_FILEMAPPING 2
#define LS_DIR 3
#define LS_LOCK 4
#define LS_COND 5
#define LS_THREAD (6 | LS_WAITABLE)
#define LS_PROC (7 | LS_WAITABLE)
#define LS_PIPE (8 | LS_WAITABLE | LS_READABLE | LS_WRITABLE)
#define LS_EVENT (9 | LS_WAITABLE)

#if LS_WINDOWS
#define LS_CLASS_FN __stdcall
#else
#define LS_CLASS_FN
#endif // LS_WINDOWS

typedef void(LS_CLASS_FN *ls_dtor_t)(void *ptr);
typedef int(LS_CLASS_FN *ls_wait_t)(void *ptr, unsigned long ms);

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
	struct ls_class *clazz;	//!< Class of the handle
	uint8_t data[0];		//!< Data of the handle
};

//! \brief Create handle from class.
//! 
//! \details Creates a new handle of the given class.
//! 
//! \param [in] clazz The class of the handle. Must be valid
//! through the lifetime of the handle.
//! 
//! \return The new handle. This is a pointer to the handle data. The
//! data will be initialized to zero.
ls_handle ls_handle_create(struct ls_class *clazz);

//! \brief Retrieve handle information.
//! 
//! \param [in] h The handle. Cannot be NULL.
//! 
//! \return The handle information.
#define ls_get_handle_info(h) ((struct ls_handle_info *)(h)-1)

#endif // _LS_HANDLE_H_
