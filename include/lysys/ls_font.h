#ifndef _LS_FONT_H_
#define _LS_FONT_H_

#include "ls_defs.h"

//! \brief Find the path to a system font.
//! 
//! Searches the system font directory for a font that best matches the
//! given name, case-insensitive.
//! 
//! \param name The name of the font
//! \param path The buffer to store the path
//! \param path_len The size of the path buffer
//! 
//! \return If path and path_len are 0, the return value is the
//! required size of the path buffer, including the null terminator.
//! If both are non-zero, the return value is the length of the path,
//! excluding the null terminator. If an error occurs -1 is returned.
//! If the font could not be found, -1 is returned and ls_errno is set
//! to LS_NOT_FOUND.
size_t ls_find_system_font(const char *name, char *path, size_t path_len);

#endif // _LS_FONT_H_
