#ifndef _LS_FILE_PRIV_H_
#define _LS_FILE_PRIV_H_

#include "ls_native.h"

typedef struct ls_file
{
#if LS_WINDOWS
	HANDLE hFile; // NULL means act as the NUL device
#else
	int fd; // -1 means act as /dev/null
#endif // LS_WINDOWS
} ls_file_t;

typedef struct ls_pipe
{
#if LS_WINDOWS
	HANDLE hPipe;
	OVERLAPPED ov;
#else
	int fd;
#endif // LS_WINDOWS
	int connected;
} ls_pipe_t;

//! \brief Resolve a file handle to a file object.
//! 
//! This performs both type checking and resolution of
//! psuedo-handles (e.g. LS_STDIN, LS_STDOUT, LS_STDERR).
//! The returned handle should not be used to access
//! the handle info, as it may not have a class.
//! 
//! \param fh The file handle to resolve.
//! \param flags A pointer to a variable that will be set
//! to the flags associated with the file handle.
//! 
//! \return A pointer to the file object, or NULL if the
//! handle is invalid.
ls_file_t *ls_resolve_file(ls_handle fh, int *flags);

ls_pipe_t *ls_resolve_pipe(ls_handle fh, int *flags);

#endif // _LS_FILE_PRIV_H_
