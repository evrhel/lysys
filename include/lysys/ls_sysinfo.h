#ifndef _LS_INFO_H_
#define _LS_INFO_H_

#include "ls_defs.h"

#define LS_ARCH_UNKNOWN 0
#define LS_ARCH_AMD64 1
#define LS_ARCH_ARM 2
#define LS_ARCH_ARM64 3
#define LS_ARCH_X86 4
#define LS_ARCH_IA64 5

struct ls_meminfo
{
    uint64_t total;
    uint64_t avail;
};

struct ls_cpuinfo
{
    int arch;
    int num_cores;
};

int ls_get_meminfo(struct ls_meminfo *mi);

int ls_get_cpuinfo(struct ls_cpuinfo *ci);

#endif // _LS_INFO_H_
