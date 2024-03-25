#include <lysys/ls_sysinfo.h>

#include "ls_native.h"

int ls_get_meminfo(struct ls_meminfo *mi)
{
#if LS_WINDOWS
    MEMORYSTATUSEX statex;
    
    statex.dwLength = sizeof(statex);
    if (!GlobalMemoryStatusEx(&statex))
        return 0;
  
    mi->total = statex.ullTotalPhys;
    mi->avail = statex.ullAvailPhys;
    return 1;
#else
    return 0;
#endif
}

int ls_get_cpuinfo(struct ls_cpuinfo *ci)
{
#if LS_WINDOWS
    SYSTEM_INFO sysinfo;

    GetSystemInfo(&sysinfo);
    ci->num_cores = sysinfo.dwNumberOfProcessors;

    switch (sysinfo.wProcessorArchitecture)
    {
    case PROCESSOR_ARCHITECTURE_AMD64:
        ci->arch = LS_ARCH_AMD64;
        break;
    case PROCESSOR_ARCHITECTURE_ARM:
        ci->arch = LS_ARCH_ARM;
        break;
    case PROCESSOR_ARCHITECTURE_ARM64:
        ci->arch = LS_ARCH_ARM64;
        break;
    case PROCESSOR_ARCHITECTURE_INTEL:
        ci->arch = LS_ARCH_X86;
        break;
    case PROCESSOR_ARCHITECTURE_IA64:
        ci->arch = LS_ARCH_IA64;
        break;
    default:
        ci->arch = LS_ARCH_UNKNOWN;
        break;
    }

    return 1;
#else
    return 0;
#endif
}
