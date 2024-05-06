#ifndef _LS_PROC_H_
#define _LS_PROC_H_

#include "ls_defs.h"

//! \brief Maximum number of command line arguments that can be passed to ls_spawn.
#define LS_MAX_ARGV 128

//! \brief Maximum number of environment variables that can be passed to ls_spawn.
#define LS_MAX_ENVP 128

#define LS_SPAWN_NONE 0

#define LS_PROC_ERROR (-1)
#define LS_PROC_RUNNING 0
#define LS_PROC_TERMINATED 1

//! \brief Information for starting a process.
struct ls_proc_start_info
{
	ls_handle hstdin;	//!< Redirect standard input.
	ls_handle hstdout;	//!< Redirect standard output.
	ls_handle hstderr;	//!< Redirect standard error.

	const char **envp;	//!< Environment variables.
	const char *cwd;	//!< Current working directory.
};

//! \brief Start a process.
//! 
//! Starts a process with the specified path and arguments. The process
//! is started in the background and the function returns immediately.
//! 
//! Use ls_wait to wait for the process to complete and ls_close to
//! close the handle when information about the process is no longer
//! needed. Closing a handle does not terminate the process.
//! 
//! Note on Unix-like systems, closing a running process handles does
//! not immediately release the resources associated with the process.
//! The process is waited on in the background and resources are
//! released when the process completes.
//! 
//! \param path Path to the executable image.
//! \param argv NULL-terminated array of arguments.
//! \param info Additional information for starting the process. Can be NULL.
//! 
//! \return Handle to the process or NULL on failure.
ls_handle ls_proc_start(const char *path, const char *argv[], const struct ls_proc_start_info *info);

//! \brief Open a process.
//! 
//! Opens a process with the specified process ID. The process must be
//! running and the process ID must be valid. The process can be
//! queried for information and signaled using the returned handle.
//! 
//! \param pid Process ID.
//! 
//! \return Handle to the process or NULL on failure.
ls_handle ls_proc_open(unsigned long pid);

//! \brief Kill a process.
//! 
//! Terminates a process with the specified process handle. The
//! handle still needs to be closed using ls_close.
//!
//! \param ph Handle to the process.
//! \param exit_code Exit code for the process.
//! 
//! \return 0 on success, -1 on failure.
int ls_kill(ls_handle ph, int exit_code);

//! \brief Check if a process is running.
//! 
//! \param ph Handle to the process.
//! 
//! \return One of LS_PROC_*.
int ls_proc_state(ls_handle ph);

//! \brief Get the exit code of a process.
//! 
//! Retrieves the exit code of a process that has terminated.
//! 
//! \param ph Handle to the process.
//! \param exit_code Pointer to the exit code. Only set if
//! the process has terminated.
//! 
//! \return 0 on success, 1 if the process is still running,
//! -1 on failure.
int ls_proc_exit_code(ls_handle ph, int *exit_code);

//! \brief Retrieve the path of the executable image of a process.
//! 
//! \param ph Handle to the process.
//! \param path Buffer to receive the path.
//! \param size Size of the buffer.
//! 
//! \return Length of the path, if path and size are non-NULL,
//! the required size of the buffer if path is NULL, or -1
//! on failure. 
size_t ls_proc_path(ls_handle ph, char *path, size_t size);

//! \brief Retrieve the name of a process.
//! 
//! This is the same as the base name of the executable image.
//! 
//! \param ph Handle to the process.
//! \param name Buffer to receive the name.
//! \param size Size of the buffer.
//! 
//! \return Length of the name, if name and size are non-NULL,
//! the required size of the buffer if name is NULL, or -1
//! on failure.
size_t ls_proc_name(ls_handle ph, char *name, size_t size);

//! \brief Retrieve the process ID of a process.
//! 
//! \param ph Handle to the process.
//! 
//! \return The process ID of the process, or -1 on failure.
unsigned long ls_getpid(ls_handle ph);

//! \brief Retrieve the process ID of the current process.
//! 
//! \return The process ID of the current process.
unsigned long ls_getpid_self(void);

//! \brief Retrieve a psuedo-handle to the current process.
//! 
//! Returns a psuedo-handle to the current process. The handle does
//! not need to be closed and is only used for querying information
//! about the current process.
//! 
//! \return A psuedo-handle to the current process.
ls_handle ls_proc_self(void);

#endif
