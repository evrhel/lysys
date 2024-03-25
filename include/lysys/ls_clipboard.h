#ifndef _LS_CLIPBOARD_H_
#define _LS_CLIPBOARD_H_

#include "ls_defs.h"

//! \brief Text clipboard format.
#define LS_CF_TEXT 1

//! \brief Register a new clipboard format.
//! 
//! Registers a new clipboard format. The format is identified by a
//! unique integer value, which is used to identify the format when
//! setting or getting clipboard data.
//! 
//! \param [in] name The name of the new or existing format.
//! 
//! \return The format identifier, or -1 if the function fails.
intptr_t ls_register_clipboard_format(const char *name);

//! \brief Set the clipboard data.
//! 
//! Sets the clipboard data to the specified format. The data is
//! copied to the clipboard, so the caller is responsible for freeing
//! the memory after the function returns.
//! 
//! \param [in] fmt The format of the data.
//! \param [in] data A pointer to the data.
//! \param [in] cb The size of the data, in bytes.
//! 
//! \return 0 on success, -1 on failure.
int ls_set_clipboard_data(intptr_t fmt, const void *data, size_t cb);

//! \brief Set the clipboard data to a string.
//! 
//! Sets the clipboard data to a string. The string is copied to the
//! clipboard, so the caller is responsible for freeing the memory
//! after the function returns.
//! 
//! \param [in] text The text to set.
//! 
//! \return 0 on success, -1 on failure.
int ls_set_clipboard_text(const char *text);

//! \brief Clear the clipboard data.
//!
//! Clears the clipboard data.
//! 
//! \return 0 on success, -1 on failure.
int ls_clear_clipboard_data(void);

//! \brief Get data from the clipboard.
//! 
//! Gets the clipboard data of the specified format. The data is
//! copied to the buffer, so the caller is responsible for freeing
//! the memory after the function returns.
//! 
//! \param [in] fmt The format of the data to get.
//! \param [out, opt] data A pointer to the buffer that will receive
//! the data. If this parameter is NULL, the function returns the size
//! of the data, in bytes.
//! \param [in] cb The maximum number of bytes to copy to the buffer.
//! Ignored if data is NULL.
//! 
//! \return The number of bytes copied to the buffer, or the size of
//! the data, in bytes, if data is NULL. Returns 0 if the clipboard
//! does not contain data of the specified format, or -1 on failure.
size_t ls_get_clipboard_data(intptr_t fmt, void *data, size_t cb);

#endif // _LS_CLIPBOARD_H_
