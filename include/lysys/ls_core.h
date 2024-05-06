#ifndef _LS_CORE_H_
#define _LS_CORE_H_

#include "ls_defs.h"

#define LS_INFINITE ((unsigned long)-1)

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

//! \brief Get the last error code.
//!
//! Returns the last error code that occurred on the calling thread.
//! The error code is specific to the calling thread and is not shared
//! between threads. The error code is reset after each function call.
//!
//! \return The last error code or 0 if no error occurred.
int ls_errno(void);

void ls_perror(const char *msg);

const char *ls_strerror(int err);

//! \brief Extract a substring.
//! 
//! Extracts n characters from the string s and copies them to the
//! buffer buf. The buffer must be large enough to store the substring,
//! including the null terminator (i.e. buf must be at least n + 1
//! bytes long). The substring is copied to the buffer and the buffer
//! is null-terminated. If n is greater than the length of s, the entire
//! string is copied to the buffer. The length of the substring copied
//! to the buffer is returned, excluding the null terminator.
//! 
//! \param s The string to extract from.
//! \param n The number of characters to extract.
//! \param buf The buffer to copy the substring to.
//! 
//! \return The length of the substring copied to the buffer, excluding
//! the null terminator. If an error occurs, -1 is returned.
size_t ls_substr(const char *s, size_t n, char *buf);

void *ls_malloc(size_t size);
void *ls_calloc(size_t count, size_t size);
void *ls_realloc(void *ptr, size_t size);
void ls_free(void *ptr);

char *ls_strdup(const char *s);

#endif // _LYSYS_H_
