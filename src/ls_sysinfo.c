#include <lysys/ls_sysinfo.h>

#include <string.h>

#include "ls_native.h"

int ls_get_meminfo(struct ls_meminfo *mi)
{
#if LS_WINDOWS
    MEMORYSTATUSEX statex;
    
    statex.dwLength = sizeof(statex);
    if (!GlobalMemoryStatusEx(&statex))
        return -1;
  
    mi->total = statex.ullTotalPhys;
    mi->avail = statex.ullAvailPhys;
    return 0;
#else
    struct sysinfo si;
    int rc;

    rc = sysinfo(&si);
    if (rc == -1) return -1;

    mi->total = si.totalram;
    mi->avail = si.freeram;
    return 0;
#endif // LS_WINDOWS
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

    return 0;
#else
    struct utsname uts;
    int rc;

    rc = uname(&uts);
    if (rc == -1) return -1;

    ci->num_cores = sysconf(_SC_NPROCESSORS_ONLN);

    if (strcmp(uts.machine, "x86_64") == 0)
        ci->arch = LS_ARCH_AMD64;
    else if (strcmp(uts.machine, "arm") == 0)
        ci->arch = LS_ARCH_ARM;
    else if (strcmp(uts.machine, "aarch64") == 0)
        ci->arch = LS_ARCH_ARM64;
    else if (strcmp(uts.machine, "i686") == 0)
        ci->arch = LS_ARCH_X86;
    else if (strcmp(uts.machine, "ia64") == 0)
        ci->arch = LS_ARCH_IA64;
    else
        ci->arch = LS_ARCH_UNKNOWN;

    return 0;
#endif // LS_WINDOWS
}
