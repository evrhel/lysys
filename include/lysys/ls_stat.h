#ifndef _LS_STAT_H_
#define _LS_STAT_H_

#include <lysys/ls_defs.h>

struct ls_stat
{
	uint64_t size;
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

#endif
