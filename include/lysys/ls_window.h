#ifndef _LS_WINDOW_H_
#define _LS_WINDOW_H_

#include "ls_defs.h"

#define LS_DIALOG_OK 0x00
#define LS_DIALOG_OKCANCEL 0x01
#define LS_DIALOG_ABORTRETRYIGNORE 0x02
#define LS_DIALOG_YESNOCANCEL 0x03
#define LS_DIALOG_YESNO 0x04
#define LS_DIALOG_RETRYCANCEL 0x05
#define LS_DIALOG_CANCELTRYCONTINUE 0x06
#define LS_DIALOG_TYPE_MASK 0x0f

#define LS_DIALOG_ERROR 0x10
#define LS_DIALOG_QUESTION 0x20
#define LS_DIALOG_WARNING 0x30
#define LS_DIALOG_INFORMATION 0x40
#define LS_DIALOG_ICON_MASK 0xf0

#define LS_CMD_OK 1
#define LS_CMD_CANCEL 2
#define LS_CMD_ABORT 3
#define LS_CMD_RETRY 4
#define LS_CMD_IGNORE 5
#define LS_CMD_YES 6
#define LS_CMD_NO 7
#define LS_CMD_CLOSE 8
#define LS_CMD_HELP 9
#define LS_CMD_TRYAGAIN 10
#define LS_CMD_CONTINUE 11

#define LS_FILE_DIALOG_DIR 0x01
#define LS_FILE_DIALOG_MUST_EXIST 0x02
#define LS_FILE_DIALOG_MULTI 0x04

typedef struct file_filter
{
    const char *name;
    const char *pattern;
} file_filter_t;

int ls_dialog_message(void *parent, const char *title, const char *message, int flags);

int ls_dialog_input(void *parent, const char *title, const char *message, char *buffer, size_t size, int flags);

//! \brief Show a file open dialog.
//!
//! \details Shows a dialog that allows the user to select a single file.
//!
//! \param [in] parent The parent window of the dialog. HWND on Windows.
//! \param [in] filters An array of file filters. If NULL, all files are shown.
//! \param [in] flags Flags that control the behavior of the dialog.
//! \param [out] results A pointer to a handle that will receive the results of the dialog.
//!
//! \return 0 if the user selected a file, 1 if the user canceled the dialog, and
//!         -1 if an error occurred.
int ls_dialog_open(void *parent, const file_filter_t *filters, int flags, ls_handle *results);

int ls_dialog_save(void *parent, const file_filter_t *filters, int flags, char *filename, size_t size);

const char *ls_dialog_next_file(ls_handle results);

#endif // _LS_WINDOW_H_
