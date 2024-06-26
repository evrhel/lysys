#ifndef _LS_FILE_H_
#define _LS_FILE_H_

#include "ls_defs.h"

//
/////////////////////////////////////////////////////////////////////
// Psuedo file handles
//

// Path to the null device (e.g. /dev/null on Unix, NUL on Windows)
#define LS_DEVNULL ((ls_handle)-9)

// Standard input
#define LS_STDIN ((ls_handle)-10)

// Standard output
#define LS_STDOUT ((ls_handle)-11)

// Standard error
#define LS_STDERR ((ls_handle)-12)

//
/////////////////////////////////////////////////////////////////////
// File access modes
//

// Read access
#define LS_FILE_READ 0x1

// Write access
#define LS_FILE_WRITE 0x2

// Execute access
#define LS_FILE_EXECUTE 0x4

// Existence check only
#define LS_FILE_EXIST 0x8

// Open the file for asynchronous I/O
// On Windows, this will cause synchronous I/O operations to fail
#define LS_FLAG_ASYNC 0x1000

// Optimize the file for random access
#define LS_FLAG_RANDOM 0x2000

// Optimize the file for sequential access
#define LS_FLAG_SEQUENTIAL 0x4000

// Child processes inherit the file handle
#define LS_FLAG_INHERIT 0x8000

// Create the read end of an anonymous pipe for asynchronous I/O
#define LS_ANON_PIPE_READ_ASYNC 0x10000

// Create the write end of an anonymous pipe for asynchronous I/O
#define LS_ANON_PIPE_WRITE_ASYNC 0x20000

// Create both ends of an anonymous pipe for asynchronous I/O
#define LS_ANON_PIPE_ASYNC (LS_ANON_PIPE_READ_ASYNC | LS_ANON_PIPE_WRITE_ASYNC)

//
/////////////////////////////////////////////////////////////////////
// File sharing modes
//

// Do not allow other processes to access the file
#define LS_SHARE_NONE 0x0

// Allow other processes to read from the file
#define LS_SHARE_READ 0x1

// Allow other processes to write to the file
#define LS_SHARE_WRITE 0x2

// Allow other processes to delete the file
#define LS_SHARE_DELETE 0x4

//
/////////////////////////////////////////////////////////////////////
// File creation modes
//

// Create a new file, failing if the file already exists
#define LS_CREATE_NEW 1

// Create a new file, overwriting the file if it already exists
#define LS_CREATE_ALWAYS 2

// Open an existing file, failing if the file does not exist
#define LS_OPEN_EXISTING 3

// Open an existing file, creating the file if it does not exist
#define LS_OPEN_ALWAYS 4

// Open an existing file, truncating the file to zero length
#define LS_TRUNCATE_EXISTING 5

//
/////////////////////////////////////////////////////////////////////
// File seek origins
//

// seek from the beginning of the file
#define LS_SEEK_SET 0

// seek from the current file pointer position
#define LS_SEEK_CUR 1

// seek from the end of the file
#define LS_SEEK_END 2

//
/////////////////////////////////////////////////////////////////////
// Asynchronous I/O status
//

// An error occurred during the asynchronous I/O operation
#define LS_AIO_ERROR -1

// The asynchronous I/O operation has completed
#define LS_AIO_COMPLETED 0

// The asynchronous I/O operation is pending
#define LS_AIO_PENDING 1

// The asynchronous I/O operation was canceled
#define LS_AIO_CANCELED 2

//
/////////////////////////////////////////////////////////////////////
// File types
//

// Unknown file type
#define LS_FT_UNKNOWN 0

// Regular file
#define LS_FT_FILE 1

// Directory
#define LS_FT_DIR 2

// Symbolic link
#define LS_FT_LINK 3

// Device file
#define LS_FT_DEV 4

// Named pipe
#define LS_FT_PIPE 5

// Socket
#define LS_FT_SOCK 6

//
/////////////////////////////////////////////////////////////////////
// Limits
//

// Maximum pipe name length, including null terminator
#define LS_MAX_PIPE_NAME 256

//
/////////////////////////////////////////////////////////////////////
//

//! \brief Open a file or I/O device
//!
//! Opens a named file or I/O device using the specified access,
//! share, and creation mode and returns a handle to the file.
//! 
//! \param [in] path The path to the file or I/O device
//! \param [in] access The access mode and flags
//! \param [in] share The sharing mode
//! \param [in] create The creation mode
//! 
//! \return A handle to the file or I/O device, or NULL if an error
//! occurred.
ls_handle ls_open(const char *path, int access, int share, int create);

//! \brief Set the file pointer position
//! 
//! Sets the file pointer position for the specified file or I/O
//! device. Subsequent synchronous read and write operations will
//! occur at the specified offset.
int64_t ls_seek(ls_handle fh, int64_t offset, int origin);

//! \brief Read from a file or I/O device
//! 
//! Performs a synchronous read operation on the specified file or
//! I/O device. The data is read from the file or device and stored
//! in the specified buffer.
//! 
//! \param fh The handle to the file or I/O device
//! \param buffer A pointer to the buffer to store the data
//! \param size The number of bytes to read
//! 
//! \return -1 if an error occurred, 0 if the end of the file was
//! reached, or the number of bytes read.
size_t ls_read(ls_handle fh, void *buffer, size_t size);

// \brief Write to a file or I/O device
//!
//! Performs a synchronous write operation on the specified file or
//! I/O device. The data in the specified buffer is written to the
//! file or device.
//! 
//! \param fh The handle to the file or I/O device
//! \param buffer A pointer to the buffer containing the data
//! \param size The number of bytes to write
//! 
//! \return -1 if an error occurred, or the number of bytes written.
size_t ls_write(ls_handle fh, const void *buffer, size_t size);

//! \brief Flush the file or I/O device
//! 
//! Flushes any buffered data to the file or I/O device.
//! 
//! \param fh The handle to the file or I/O device
//! 
//! \return 0 if the data was successfully flushed, -1 if an error
//! occurred.
int ls_flush(ls_handle fh);

//! \brief Queue an asynchronous read operation
//! 
//! Queues an asynchronous read operation on the specified file or
//! I/O device. The operation will read the specified number of
//! bytes from the file or device starting at the specified offset
//! and store the data in the specified buffer. The buffer must
//! remain valid until the asynchronous I/O request completes.
//! 
//! On Windows, the size parameter is clamped to 2^32 - 1 (4GB),
//! call ls_aio_read multiple times for larger files.
//! 
//! \param aioh The handle to the asynchronous I/O request
//! \param offset The offset in the file or device to read from
//! \param buffer A pointer to the buffer to store the data
//! \param size The number of bytes to read
//! 
//! \return -1 if an error occurred, 0 if the request was queued
int ls_aio_read(ls_handle aioh, uint64_t offset, volatile void *buffer, size_t size);

//! \brief Queue an asynchronous write operation
//! 
//! Queues an asynchronous write operation on the specified file or
//! I/O device. The operation will write the specified number of
//! bytes to the file or device starting at the specified offset
//! using the data in the specified buffer. The buffer must remain
//! valid until the asynchronous I/O request completes.
//! 
//! On Windows, the size parameter is clamped to 2^32 - 1 (4GB),
//! call ls_aio_write multiple times for larger files.
//! 
//! \param aioh The handle to the asynchronous I/O request
//! \param offset The offset in the file or device to write to
//! \param buffer A pointer to the buffer containing the data
//! \param size The number of bytes to write
//! 
//! \return -1 if an error occurred, 0 if the request was queued
int ls_aio_write(ls_handle aioh, uint64_t offset, const volatile void *buffer, size_t size);

//! \brief Check the status of an asynchronous I/O request
//!
//! Checks the status of an asynchronous I/O request and returns
//! the actual number of bytes transferred, if the request has
//! completed.
//! 
//! \param aioh The handle to the asynchronous I/O request
//! \param transferred A pointer to a variable that will receive
//! the number of bytes transferred
//! 
//! \return The status of the asynchronous I/O request, one of
//! LS_AIO_ERROR, LS_AIO_PENDING, LS_AIO_COMPLETED, or
//! LS_AIO_CANCELED.
int ls_aio_status(ls_handle aioh, size_t *transferred);

//! \brief Cancel an asynchronous I/O request
//! 
//! Cancels an asynchronous I/O request that is currently pending.
//! Calls to ls_aio_status will return LS_AIO_CANCELED for the
//! request. The state of the file or I/O device is undefined after
//! the request is canceled.
//! 
//! \param aioh The handle to the asynchronous I/O request
//! 
//! \return 0 if the request was successfully canceled, -1 if an
//! error occurred.
int ls_aio_cancel(ls_handle aioh);

//! \brief Move a file
//! 
//! Moves a file from the old path to the new path.
//! 
//! \param old_path The path to the file to move
//! \param new_path The new path for the file
//! 
//! \return 0 if the file was successfully moved, -1 if an error
//! occurred.
int ls_move(const char *old_path, const char *new_path);

//! \brief Copy a file
//! 
//! Copies a file from the old path to the new path.
//! 
//! \param old_path The path to the file to copy
//! \param new_path The new path for the file
//! 
//! \return 0 if the file was successfully copied, -1 if an error
//! occurred.
int ls_copy(const char *old_path, const char *new_path);

//! \brief Delete a file
//! 
//! Deletes a file from the file system.
//! 
//! \param path The path to the file to delete
//! 
//! \return 0 if the file was successfully deleted, -1 if an error
//! occurred.
int ls_delete(const char *path);

//! \brief Create a file
//! 
//! Creates a new file with the specified path and size, filling
//! it with zeros.
//! 
//! \param path The path to the file to create
//! \param size The size of the file in bytes
//! 
//! \return 0 if the file was successfully created, -1 if an error
int ls_createfile(const char *path, size_t size);

//! \brief Create a directory
//! 
//! Creates a new directory with the specified path.
//! 
//! \param path The path to the directory to create
//! 
//! \return 0 if the directory was successfully created, -1 if an
//! error occurred.
int ls_createdir(const char *path);

//! \brief Create a directory and any intermediate directories
//! 
//! Creates a new directory with the specified path and any
//! intermediate directories that do not exist. If an error
//! occurs, intermediate directories may still have been created.
//! 
//! \param path The path to the directory to create
//! 
//! \return 0 if the directory was successfully created, -1 if an
//! error occurred.
int ls_createdirs(const char *path);

//! \brief Create an anonymous pipe
//! 
//! If a pipe end is opened for asynchronous I/O, it cannot be used
//! for process I/O redirection.
//! 
//! \param read A pointer to a handle that will receive the read end
//! of the pipe
//! \param write A pointer to a handle that will receive the write
//! end of the pipe
//! \param flags Flags for the pipe creation. Use a combination of
//! LS_ANON_PIPE_WRITE_ASYNC and LS_ANON_PIPE_READ_ASYNC to create
//! an asynchronous pipe.
//! 
//! \return 0 if the pipe was successfully created, -1 if an error
//! occurred.
int ls_pipe(ls_handle *read, ls_handle *write, int flags);

//! \brief Create a named pipe.
//! 
//! \param name Name of the pipe.
//! \param flags Pipe creation flags.
//! \param wait Whether to wait for a client to connect to the pipe.
//! If the pipe is asynchronous, this parameter is ignored.
//!
//! \return Handle to the named pipe, or NULL if an error occurred.
ls_handle ls_named_pipe(const char *name, int flags, int wait);

//! \brief Wait for a named pipe to connect.
//! 
//! \param fh Handle to the named pipe.
//! \param timeout Timeout in milliseconds. Use 0 to check if the
//! pipe is connected without waiting. If the pipe is not asynchronous,
//! and this parameter is nonzero, the function will block indefinitely
//! until the pipe is connected or an error occurs.
//! 
//! \return 0 if the pipe is connected, -1 if an error occurred,
//! or 1 if the timeout expired.
int ls_named_pipe_wait(ls_handle fh, unsigned long timeout);

ls_handle ls_pipe_open(const char *name, int access, unsigned long timeout);

#endif // _LS_FILE_H_
