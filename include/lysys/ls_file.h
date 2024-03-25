#ifndef _LS_FS_H_
#define _LS_FS_H_

#include "ls_defs.h"

#if LS_WINDOWS
#define LS_ASYNC_IO_RESERVED_SIZE 32 // 32 bytes for OVERLAPPED structure
#else
#define LS_ASYNC_IO_RESERVED_SIZE 0
#endif

// File access modes

#define LS_A_READ 1
#define LS_A_WRITE 2
#define LS_A_EXECUTE 4

#define LS_A_ASYNC 0x10000
#define LS_A_RANDOM 0x20000
#define LS_A_SEQUENTIAL 0x40000

// File sharing modes

#define LS_S_NONE 0
#define LS_S_READ 1
#define LS_S_WRITE 2
#define LS_S_DELETE 4

// File creation modes

#define LS_CREATE_NEW 1
#define LS_CREATE_ALWAYS 2
#define LS_OPEN_EXISTING 3
#define LS_OPEN_ALWAYS 4
#define LS_TRUNCATE_EXISTING 5

// File seek origins

#define LS_O_BEGIN 0
#define LS_O_CURRENT 1
#define LS_O_END 2

// Asynchronous I/O status

#define LS_IO_ERROR -1
#define LS_IO_PENDING 0
#define LS_IO_COMPLETED 1
#define LS_IO_CANCELED 2

// File types

#define LS_FT_UNKNOWN 0
#define LS_FT_FILE 1
#define LS_FT_DIR 2
#define LS_FT_LINK 3
#define LS_FT_DEV 4
#define LS_FT_PIPE 5
#define LS_FT_SOCK 6

//! \brief Asynchronous I/O structure
//! 
//! Stores information about an asynchronous I/O operation. This
//! structure is passed to various I/O functions to perform
//! asynchronous I/O operations. The structure must remain valid
//! until the operation has completed.
struct ls_async_io
{
	uintptr_t status;	//!< Status of the operation
	size_t transferred;	//!< Number of bytes transferred
	ls_handle event;	//!< Event to signal when complete
	uint64_t offset;	//!< Starting offset of the operation

	uint8_t reserved[LS_ASYNC_IO_RESERVED_SIZE];
};

//! \brief Open a file or I/O device
//!
//! Opens a named file or I/O device using the specified access,
//! share, and creation mode and returns a handle to the file.
//! 
//! \param [in] path The path to the file or I/O device
//! \param [in] access The access mode, can be a combination of one
//! more of the ls_Access flags.
//! \param [in] share The sharing mode, can be a combination of one
//! or more of the ls_Share flags. The sharing mode prevents other
//! processes from opening the file or device if the requested
//! sharing mode conflicts with the access mode. This parameter
//! is only used on Windows.
//! \param [in] create The creation mode, can be one of the
//! ls_Create flags.
//! 
//! \return A handle to the file or I/O device, or NULL if an error
//! occurred.
ls_handle ls_open(const char *path, int access, int share,
	int create);

int64_t ls_seek(ls_handle file, int64_t offset, int origin);

size_t ls_read(ls_handle file, void *buffer, size_t size,
	struct ls_async_io *async);

size_t ls_write(ls_handle file, const void *buffer, size_t size,
	struct ls_async_io *async);

int ls_flush(ls_handle file);

int ls_get_async_io_result(ls_handle file, struct ls_async_io *async,
	unsigned long ms);

int ls_cancel_async_io(ls_handle file, struct ls_async_io *async);

int ls_move(const char *old_path, const char *new_path);

int ls_copy(const char *old_path, const char *new_path);

int ls_delete(const char *path);

int ls_createfile(const char *path, size_t size);

int ls_createdir(const char *path);

int ls_createdirs(const char *path);

#endif
