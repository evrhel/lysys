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

ls_handle ls_spawn(const char *path, const char *argv[], const char *envp[], const char *cwd, int flags);

ls_handle ls_spawn_shell(const char *cmd, const char *envp[], const char *cwd, int flags);

ls_handle ls_proc_open(unsigned long pid);

void ls_kill(ls_handle ph, int signum);

int ls_proc_state(ls_handle ph);

int ls_proc_exit_code(ls_handle ph, int *exit_code);

size_t ls_proc_path(ls_handle ph, char *path, size_t size);

size_t ls_proc_name(ls_handle ph, char *name, size_t size);

unsigned long ls_getpid(ls_handle ph);

unsigned long ls_getpid_self(void);

ls_handle ls_proc_self(void);

unsigned long ls_proc_parent(unsigned long pid);

ls_handle ls_pipe_create(const char *name);

ls_handle ls_pipe_open(const char *name, unsigned long ms);

#endif
