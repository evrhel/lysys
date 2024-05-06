#ifndef _LS_STAT_H_
#define _LS_STAT_H_

#include <lysys/ls_defs.h>

struct ls_stat
{
	uint64_t size;
	uint64_t ctime;
	uint64_t atime;
	uint64_t mtime;
	int type;
};

struct ls_dir
{
	uint64_t size;
	char *name;
	int type;
};

int ls_stat(const char *path, struct ls_stat *st);

int ls_fstat(ls_handle file, struct ls_stat *st);

int ls_access(const char *path, int mode);

ls_handle ls_opendir(const char *path);

struct ls_dir *ls_readdir(ls_handle dir);

//! \brief Create a snapshot of a directory tree
//! 
//! A snapshot captures the state of a directory tree at a given
//! point in time and can be iterated through. Taking a snapshot is
//! expensive and scales with the size of the directory tree.
//!
//! \param path Path to a directory
//! \param flags Snapshot flahs
//! \param max_depth Maximum number of levels to traverse
//! (-1=no limit)
//!
//! \return A handle to the snapshot
ls_handle ls_snapshot_dir(const char *path, int flags, uint32_t max_depth);

//! \brief Retrieve the absolute path of the snapshot
//! 
//! \param ssh A snapshot
//! \param path Buffer to store the path in
//! \param size Size of the buffer
//! 
//! \return If name and size are 0, the number of bytes required
//! to store the path (including the null terminator), otherwise
//! the length of the name on success, -1 on error
size_t ls_snapshot_path(ls_handle ssh, char *path, size_t size);

//! \brief Retrieve the file name of the snapshot
//!
//! \param ssh A snapshot
//! \param name Buffer to store the name in
//! \param size Size of the buffer
//!
//! \return If name and size are 0, the number of bytes required
//! to store the name (including the null terminator), otherwise
//! the length of the name on success, -1 on error
size_t ls_snapshot_name(ls_handle ssh, char *name, size_t size);

//! \brief Retrieve information about a file in a snapshot
//!
//! \param ssh A snapshot
//! \param st Structure to store information in
//!
//! \return 0 on success, -1 on error
int ls_snapshot_stat(ls_handle ssh, struct ls_stat *st);

//! \brief Iterate through a snapshot's directory entries
//! 
//! The returned handle must not be closed. If closed, the snapshot
//! is in an undefined state.
//!
//! \param ssh A snapshot
//! \param it A pointer to an interator. Cannot be NULL. To begin
//! iteration, set the value of *it to NULL.
//!
//! \return A snapshot of the subtree
ls_handle ls_snapshot_enumerate(ls_handle ssh, void **it);

//! \brief Lookup a file in a snapshot
//! 
//! The returned handle must not be closed. If closed, the snapshot
//! is in an undefined state.
//! 
//! \param ssh A snapshot
//! \param path The path to the file
//! 
//! \return A handle to the file, or NULL if not found or on error
ls_handle ls_snapshot_lookup(ls_handle ssh, const char *path);

//! \brief Refresh a snaphot
//! 
//! Will update the snapshot to reflect the current state of the
//! directory tree. Any handles to files or directories in the
//! snapshot will be invalidated. If any changes are detected, the
//! function will report them through the specified callback.
//!
//! \param ssh The snapshot to refresh
//! \param max_depth Maximum number of levels to traverse, -1=no limit
//! \param change_cb Callback function to call when a change is detected,
//! uses the same constants as in ls_watch.h. Do not query the snapshot
//! from within the callback.
//! \param up User pointer to pass to the callback
//!
//! \return 0 on success, -1 on error
int ls_snapshot_refresh(ls_handle ssh, uint32_t max_depth,
	void(*cb)(const char *path, int event, void *up), void *up);

#endif
