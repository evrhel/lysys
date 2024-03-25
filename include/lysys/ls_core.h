#ifndef _LS_CORE_H_
#define _LS_CORE_H_

#include "ls_defs.h"

#define LS_INFINITE ((unsigned long)-1)

typedef void (*ls_exit_hook_t)(int);

struct ls_allocator
{
	void *(*malloc)(size_t size);
	void *(*calloc)(size_t nmemb, size_t size);
	void *(*realloc)(void *ptr, size_t size);
	void (*free)(void *ptr);
};

void ls_init(const struct ls_allocator *allocator);

void ls_shutdown(void);

int ls_add_exit_hook(ls_exit_hook_t hook);

//! \brief Exit the program.
//!
//! Exits the program with the specified status code. This function
//! will call any registered exit hooks before the program exits.
//!
//! \param [in] status The status code to exit with.
void ls_exit(int status);

//! \brief Wait for a handle to become signaled.
//! 
//! Waits for the specified handle to become signaled. If the handle
//! is already signaled, the function returns immediately. If the handle
//! is not signaled, the function blocks until the handle is signaled
//! or an error occurs. Only some types of handles can be waited on (e.g.
//! threads, processes, events, etc.). If the handle is not of a type that
//! can be waited on, the function will fail.
//! 
//! \param [in] h The handle to wait for.
//! 
//! \return 0 if the handle is signaled, -1 if an error occurred. Use
//! ls_error() to see additional error information.
int ls_wait(ls_handle h);

//! \brief Wait for a handle to become signaled, with a timeout.
//! 
//! Follows the same behavior as ls_wait(), but with a timeout. If
//! the handle is not signaled within the specified time, the
//! function will return early.
//! 
//! \param [in] h The handle to wait for.
//! \param [in] ms The timeout in milliseconds.
//! 
//! \return 0 if the handle is signaled, -1 if an error occurred, or
//! 1 if the timeout expired. Use ls_error() to see additional error
//! information.
int ls_timedwait(ls_handle h, unsigned long ms);

//! \brief Close a handle.
//! 
//! Releases the resources associated with the handle. The handle
//! will no longer be valid after this function is called and any
//! further use of the handle will result in undefined behavior.
//! 
//! \param [in] h The handle to close.
void ls_close(ls_handle h);

void *ls_malloc(size_t size);

void *ls_calloc(size_t nmemb, size_t size);

void *ls_realloc(void *ptr, size_t size);

void ls_free(void *ptr);

char *ls_strdup(const char *s);

size_t ls_substr(const char *s, size_t n, char *buf, size_t size);

#endif // _LYSYS_H_
