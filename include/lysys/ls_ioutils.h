#ifndef _LS_IOUTILS_H_
#define _LS_IOUTILS_H_

#include <stdarg.h>

#include "ls_defs.h"

//! \brief Read all bytes from a file handle
//! 
//! Reads all bytes from a file handle and returns a pointer to the
//! data. Release the data with free() when done.
//! 
//! If an error occurs, NULL is returned and the contents of size are
//! undefined.
//! 
//! \param fh The file handle to read from
//! \param size A pointer to a size_t variable that will be set to the
//! size of the data read
//! 
//! \return A pointer to the data read, or NULL on error
void *ls_read_all_bytes(ls_handle fh, size_t *size);

//! \brief Read a line from a file handle
//! 
//! Reads a line from a file handle and returns a pointer to the
//! data. Release the data with free() when done.
//! 
//! Lines are terminated by a newline character, which is not included
//! in the returned data. If the file ends without a newline character,
//! the last line is still returned. Carrige returns and null characters
//! are ignored and will not be included in the returned data.
//! 
//! If an error occurs, NULL is returned and the contents of size are
//! undefined.
//! 
//! \param fh The file handle to read from
//! \param size A pointer to a size_t variable that will be set to the
//! length of the line (i.e. strlen() of the returned data)
//! 
//! \return A pointer to the data read, or NULL on error
char *ls_readline(ls_handle fh, size_t *len);

//! \brief Read a file into memory
//! 
//! Reads a file into memory and returns a pointer to the data. Release
//! the data with free() when done.
//! 
//! If an error occurs, NULL is returned and the contents of size are
//! undefined.
//! 
//! \param filename The name of the file to read
//! \param size A pointer to a size_t variable that will be set to the
//! size of the data read
//! 
//! \return A pointer to the data read, or NULL on error
void *ls_read_file(const char *filename, size_t *size);

//! \brief Write data to a file
//! 
//! Writes data to a file. If the file does not exist, it is created.
//! If the file does exist, it is truncated to zero length.
//! 
//! \param filename The name of the file to write to
//! \param data A pointer to the data to write
//! \param size The size of the data to write
//! 
//! \return The number of bytes written, or -1 on error
size_t ls_write_file(const char *filename, const void *data, size_t size);

size_t ls_fprintf(ls_handle fh, const char *format, ...);

size_t ls_vfprintf(ls_handle fh, const char *format, va_list args);

#endif // _LS_IOUTILS_H_
