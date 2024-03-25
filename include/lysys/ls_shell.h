#ifndef _LS_SHELL_H_
#define _LS_SHELL_H_

#include "ls_defs.h"

#if LS_WINDOWS
#define LS_PATH_SEP '\\'
#endif

char *ls_strdir(const char *path);

char *ls_strrdir(const char *path);

size_t ls_dirname(const char *path, char *buf, size_t size);

size_t ls_basename(const char *path, char *buf, size_t size);

size_t ls_getenv_buf(const char *name, char *buf, size_t size);

char *ls_getenv(const char *name);

size_t ls_expand_env(const char *src, char *dst, size_t size);

size_t ls_which(const char *path, char *buf, size_t size);

size_t ls_abspath(const char *path, char *buf, size_t size);

size_t ls_relpath(const char *path, const char *base, char *buf, size_t size);

size_t ls_realpath(const char *path, char *buf, size_t size);

#endif // _LS_SHELL_H_
