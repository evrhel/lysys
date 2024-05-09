#ifndef _LS_SHELL_H_
#define _LS_SHELL_H_

#include "ls_defs.h"

#if LS_WINDOWS
#define LS_PATH_SEP '\\'
#define LS_PATH_SEP_STR "\\"
#else
#define LS_PATH_SEP '/'
#define LS_PATH_SEP_STR "/"
#endif // LS_WINDOWS

//! \brief Find the first occurrence of the path separator in a string.
//! 
//! Finds the first occurrence of the path separator in a string and
//! returns a pointer to that position. If the path separator is not
//! found, NULL is returned.
//! 
//! \param path The string to search
//! 
//! \return A pointer to the first occurrence of the path separator, or
//! NULL if the path separator is not found
char *ls_strdir(const char *path);

//! \brief Find the last occurrence of the path separator in a string.
//! 
//! Finds the last occurrence of the path separator in a string and
//! returns a pointer to that position. If the path separator is not
//! found, NULL is returned.
//! 
//! \param path The string to search
//! 
//! \return A pointer to the last occurrence of the path separator, or
//! NULL if the path separator is not found
char *ls_strrdir(const char *path);

void ls_path_win32(char *path, size_t len);

void ls_path_unix(char *path, size_t len);

void ls_path_native(char *path, size_t len);

//! \brief Get the directory name of a path.
//! 
//! Extracts the directory name from a path. The directory name is the
//! portion of the path that precedes the last path separator. If the
//! path does not contain a path separator, the directory name is an
//! empty string. The directory name is copied to the buffer. The buffer
//! must be large enough to store the directory name, including the null
//! terminator. To determine the required size of the buffer, pass NULL
//! for buf and 0 for size. The returned path will not contain a trailing
//! path separator.
//! 
//! \param path The path to extract the directory name from
//! \param buf The buffer to store the directory name
//! \param size The size of the buffer
//! 
//! \return The length of the directory name, excluding the null terminator.
//! If buf is NULL and size is 0, the return value is the number of bytes
//! required to store the directory name, including the null terminator. If
//! an error occurs, -1 is returned.
size_t ls_dirname(const char *path, char *buf, size_t size);

//! \brief Get the base name of a path.
//! 
//! Extracts the base name from a path. The base name is the portion of
//! the path that follows the last path separator. If the path does not
//! contain a path separator, the base name is the entire path. The base
//! name is copied to the buffer. The buffer must be large enough to store
//! the base name, including the null terminator. To determine the required
//! size of the buffer, pass NULL for buf and 0 for size.
//! 
//! \param path The path to extract the base name from
//! \param buf The buffer to store the base name
//! \param size The size of the buffer
//! 
//! \return The length of the base name, excluding the null terminator. If
//! buf is NULL and size is 0, the return value is the number of bytes
//! required to store the base name, including the null terminator. If an
//! error occurs, -1 is returned.
size_t ls_basename(const char *path, char *buf, size_t size);

size_t ls_getenv_buf(const char *name, char *buf, size_t size);

char *ls_getenv(const char *name);

size_t ls_expand_env(const char *src, char *dst, size_t size);

size_t ls_which(const char *path, char *buf, size_t size);

//! \brief Get the absolute path of a file or directory.
//!
//! Converts a relative path to an absolute path. If the path is already
//! absolute, it is copied to the buffer. The buffer must be large enough
//! to store the absolute path, including the null terminator. To determine
//! the required size of the buffer, pass NULL for buf and 0 for size.
//!
//! \param path The path to the file or directory.
//! \param buf The buffer to store the absolute path.
//! \param size The size of the buffer.
//!
//! \return The length of the absolute path, excluding the null terminator.
//! If buf is NULL and size is 0, the return value is the number of bytes
//! required to store the absolute path, including the null terminator. -1
//! is returned if an error occurs.
size_t ls_abspath(const char *path, char *buf, size_t size);

size_t ls_relpath(const char *path, const char *base, char *buf, size_t size);

size_t ls_realpath(const char *path, char *buf, size_t size);

size_t ls_cwd(char *buf, size_t size);

int ls_shell_move( const char *src, const char *dst );

int ls_shell_copy( const char *src, const char *dst );

int ls_shell_delete( const char *path );

int ls_shell_recycle( const char *path );

#endif // _LS_SHELL_H_
