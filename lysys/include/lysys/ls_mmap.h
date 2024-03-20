#ifndef _LS_MMAP_H_
#define _LS_MMAP_H_

#include <lysys/ls_defs.h>

void *ls_mmap(ls_handle file, size_t size, size_t offset, int protect, ls_handle *map);

int ls_munmap(ls_handle map, void *addr);

#endif
