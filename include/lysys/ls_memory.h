#ifndef _LS_MEMORY_H_
#define _LS_MEMORY_H_

#include <lysys/ls_defs.h>

#define LS_PROT_NONE 0
#define LS_PROT_READ 1
#define LS_PROT_WRITE 2
#define LS_PROT_WRITECOPY 4
#define LS_PROT_EXEC 8

size_t ls_page_size(void);

int ls_protect(void *ptr, size_t size, int protect);

#endif // _LS_MEMORY_H_
