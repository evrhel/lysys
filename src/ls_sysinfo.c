#include <lysys/ls_sysinfo.h>

#include <string.h>

#include <lysys/ls_core.h>

#include "ls_native.h"

#if LS_WINDOWS
static PDH_HQUERY _query = NULL;
static PDH_HCOUNTER _cpu_usage_counter = NULL;
static PDH_HCOUNTER _gpu_usage_counter = NULL;
static PDH_HCOUNTER _gpu_vram_usage_counter = NULL;

static int ls_match_string(LPCWSTR lpStr, LPCWSTR lpPattern)
{
    if (!*lpPattern)
        return !!*lpStr;

    if (*lpPattern == L'*')
    {
        for (; *lpStr; ++lpStr)
        {
            if (!ls_match_string(lpStr, lpPattern + 1))
                return 0;
        }
        return ls_match_string(lpStr, lpPattern + 1);
    }

    for (; *lpPattern && *lpStr; *lpPattern)
    {
        if (*lpPattern == L'*')
        {
            if (!ls_match_string(lpStr, lpPattern))
                return 0;
        }
        else if (*lpPattern != L'?' && *lpPattern != *lpStr)
            return 1;
        else
            lpStr++;
    }

    return *lpPattern && *lpStr; // both strings must be at the end
}

static WCHAR *ls_search_for_instance(PZZWSTR mszCounterList, const WCHAR *szInstance)
{
    PZZWSTR mszPtr;
    for (mszPtr = mszCounterList; *mszPtr != 0; mszPtr += lstrlenW(mszPtr) + 1)
    {
        if (!ls_match_string(mszPtr, szInstance))
            return mszPtr;
    }

    return NULL;
}

static int ls_add_counter(HQUERY hQuery, LPCWSTR szObject, LPCWSTR szInstance, LPCWSTR szCounter, PDH_HCOUNTER *phCounter)
{
    PDH_STATUS rc;
    DWORD dwNumInstances = 0, dwNumCounters = 0;
    PZZWSTR mszCounterList, mszInstanceList;
    PWSTR szObjectInst;
    WCHAR aBuffer[1024];
    HRESULT hr;

    *phCounter = NULL;

    rc = PdhEnumObjectItemsW(
        NULL,
        NULL,
        szObject,
        NULL,
        &dwNumInstances,
        NULL,
        &dwNumCounters,
        PERF_DETAIL_NOVICE,
        0);
    if (rc != PDH_MORE_DATA)
        return -1;

    mszCounterList = ls_malloc(dwNumCounters * sizeof(WCHAR));
    if (!mszCounterList)
        return -1;

    mszInstanceList = ls_malloc(dwNumInstances * sizeof(WCHAR));
    if (!mszInstanceList)
    {
        ls_free(mszCounterList);
        return -1;
    }

    rc = PdhEnumObjectItemsW(
        NULL,
        NULL,
        szObject,
        mszInstanceList,
        &dwNumInstances,
        mszCounterList,
        &dwNumCounters,
        PERF_DETAIL_NOVICE,
        0);
    free(mszInstanceList);

    if (rc != ERROR_SUCCESS)
    {
        free(mszCounterList);
        return -1;
    }

    szObjectInst = ls_search_for_instance(mszCounterList, szInstance);
    if (szObjectInst)
    {
        hr = StringCbPrintfW(aBuffer, sizeof(aBuffer), L"\\%s(%s)\\%s",
            szObject, szObjectInst, szCounter);
        if (!SUCCEDED(hr))
        {
            free(mszCounterList);
            return -1;
        }

        rc = PdhAddCounterW(hQuery, aBuffer, 0, phCounter);
        if (rc != ERROR_SUCCESS)
        {
            free(mszCounterList);
            *phCounter = NULL;
            return -1;
        }

        free(mszCounterList);
        return 0;
    }

    free(mszCounterList);
    return -1;
}

#endif // LS_WINDOWS

int ls_init_sysinfo(void)
{
#if LS_WINDOWS
    PDH_STATUS rc;
    DWORD dwPid;
    int c;
    WCHAR szPattern[32];
    HRESULT hr;

    if (_query)
        return -1;

    // Collect information using PDH
    // Use perfmon.exe to find the counter names

    rc = PdhOpenQueryW(NULL, 0, &_query);
    if (rc != ERROR_SUCCESS)
        return -1;

    dwPid = GetCurrentProcessId();

    // CPU usage counter
    rc = PdhAddCounter(
            _query,
            L"\\Processor Information(_Total)\\% Processor Time",
            0,
            &_cpu_usage_counter);
    if (rc != ERROR_SUCCESS)
    {
        PdhCloseQuery(_query), _query = NULL;
        return -1;
    }

    // GPU usage counter
    hr = StringCbPrintfW(szPattern, sizeof(szPattern), L"pid_%d_*engtype_3D", dwPid);
    if (!SUCCEEDED(hr))
    {
        PdhCloseQuery(_query), _query = NULL;
        return -1;
    }

    c = ls_add_counter(_query, L"GPU Engine", szPattern,
        L"Utilization Percentage", &_gpu_usage_counter);
    if (c == -1)
    {
        PdhCloseQuery(_query), _query = NULL;
        return -1;
    }

    // GPU VRAM usage counter
    hr = StringCbPrintfW(szPattern, sizeof(szPattern), L"pid_%d*", dwPid);

    c = ls_add_counter(_query, L"GPU Process Memory", szPattern,
        L"Dedicated Usage", &_gpu_vram_usage_counter);
    if (c == -1)
    {
        PdhCloseQuery(_query), _query = NULL;
        return -1;
    }

    return 0;
#else
    return 0;
#endif // LS_WINDOWS
}

void ls_deinit_sysinfo(void)
{
#if LS_WINDOWS
    if (_query)
        PdhCloseQuery(_query), _query = NULL;
    _gpu_vram_usage_counter = NULL;
    _gpu_usage_counter = NULL;
    _cpu_usage_counter = NULL;
#else
#endif // LS_WINDOWS
}

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

int ls_getmetrics(struct ls_sysmetrics *sm)
{
#if LS_WINDOWS
    PDH_FMT_COUNTERVALUE pdhCpuValue, pdhGpuValue, pdhVramUsageValue;
    PROCESS_MEMORY_COUNTERS pmc;
    BOOL b;
    PDH_FUNCTION ps;

    if (!_query)
        return -1;

    // Collect data from the query

    ps = PdhCollectQueryData(_query);
    if (ps != ERROR_SUCCESS)
        return -1;

    ps = PdhGetFormattedCounterValue(_cpu_usage_counter, PDH_FMT_LONG, NULL, &pdhCpuValue);
    if (ps != ERROR_SUCCESS)
        return -1;

    ps = PdhGetFormattedCounterValue(_gpu_usage_counter, PDH_FMT_LONG, NULL, &pdhGpuValue);
    if (ps != ERROR_SUCCESS)
        return -1;

    ps = PdhGetFormattedCounterValue(_gpu_vram_usage_counter, PDH_FMT_LONG, NULL, &pdhVramUsageValue);
    if (ps != ERROR_SUCCESS)
        return -1;

    b = GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
    if (!b)
        return -1;

    // Fill the structure

    sm->cpu_usage = pdhCpuValue.longValue;
    sm->mem_usage = pmc.WorkingSetSize / 1024 / 1024;

    sm->gpu_usage = pdhGpuValue.longValue;
    sm->vram_usage = pdhVramUsageValue.longValue / 1024 / 1024;

    return 0;
#else
    return -1;
#endif // LS_WINDOWS
}

int ls_get_batteryinfo(struct ls_batteryinfo *bi)
{
#if LS_WINDOWS
    SYSTEM_POWER_STATUS sps;
    BOOL b;

    b = GetSystemPowerStatus(&sps);
    if (!b)
    {
        bi->status = LS_BATTERY_UNKNOWN;
        bi->charge = 0;
        return 0;
    }

    if (sps.BatteryFlag & 128)
    {
        bi->status = LS_BATTERY_NO_BATTERY;
        bi->charge = 0;
    }
    else if (sps.ACLineStatus == 0)
    {
        bi->status = LS_BATTERY_DISCHARGING;
        bi->charge = sps.BatteryLifePercent;
    }
    else if (sps.ACLineStatus == 1)
    {
        bi->status = LS_BATTERY_CHARGING;
        bi->charge = sps.BatteryLifePercent;
    }
    else
    {
        bi->status = LS_BATTERY_UNKNOWN;
        bi->charge = 0;
    }  

    return 0;
#else
    return -1;
#endif // LS_WINDOWS
}
