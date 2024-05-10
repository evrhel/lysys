#ifndef _LS_STRING_H_
#define _LS_STRING_H_

#include <stddef.h>
#include <string.h>

#include "ls_defs.h"

//! \brief Convert a UTF-8 string to a wide character string
//!
//! \param utf8 UTF-8 string
//! \param buf Buffer to write wide character string
//! \param cchbuf Size of buffer in characters
//! 
//! \return If buf and cchbuf are both non-zero, returns the number of characters
//! written to buf, excluding the null terminator. If buf and cchbuf are both 0,
//! returns the number of characters required to store the wide character string,
//! including the null terminator. If an error occurs, returns -1.
size_t ls_utf8_to_wchar_buf(const char *utf8, wchar_t *buf, size_t cchbuf);

wchar_t *ls_utf8_to_wchar(const char *utf8);

//! \brief Convert a wide character string to a UTF-8 string
//! 
//! \param wstr Wide character string
//! \param buf Buffer to write UTF-8 string
//! \param cbbuf Size of buffer in bytes
//! 
//! \return If buf and cbbuf are both non-zero, returns the number of bytes
//! written to buf, excluding the null terminator. If buf and cbbuf are both 0,
//! returns the number of bytes required to store the UTF-8 string, including
//! the null terminator. If an error occurs, returns -1.
size_t ls_wchar_to_utf8_buf(const wchar_t *wstr, char *buf, size_t cbbuf);

char *ls_wchar_to_utf8(const wchar_t *wstr);

#endif // _LS_STRING_H_
