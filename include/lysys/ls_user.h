#ifndef _LS_USER_H_
#define _LS_USER_H_

#include <lysys/ls_defs.h>

#define LS_DIR_USER_HOME 0x01
#define LS_DIR_USER_DOCUMENTS 0x02
#define LS_DIR_USER_PICTURES 0x03
#define LS_DIR_USER_MUSIC 0x04
#define LS_DIR_USER_VIDEOS 0x05
#define LS_DIR_USER_DOWNLOADS 0x06
#define LS_DIR_USER_DESKTOP 0x07
#define LS_DIR_USER_TEMPLATES 0x08
#define LS_DIR_USER_PUBLIC 0x09

#define LS_DIR_WINDOWS 0x1001
#define LS_DIR_SYSTEM32 0x1002
#define LS_DIR_PROGRAM_FILES 0x1003
#define LS_DIR_PROGRAM_FILES_X86 0x1004

#define LS_COMPUTER_NAME_NETBIOS 0
#define LS_COMPUTER_NAME_DNS 1

size_t ls_username(char *name, size_t size);

size_t ls_home(char *path, size_t size);

//! \brief Get the location of a common directory.
//!
//! \param dir The directory to get.
//! \param path The buffer to store the directory path.
//! \param size The size of the buffer.
//!
//! \return The length of the directory path, excluding the null terminator.
//! If path was NULL and size was 0, the return value is the number of bytes
//! required to store the directory path, including the null terminator. -1
//! is returned if an error occurs.
size_t ls_common_dir(int dir, char *path, size_t size);

size_t ls_computer_name(int type, char *name, size_t size);

#endif // _LS_USER_H_
