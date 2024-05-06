#ifndef _LS_INFO_H_
#define _LS_INFO_H_

#include "ls_defs.h"

#define LS_ARCH_UNKNOWN 0
#define LS_ARCH_AMD64 1
#define LS_ARCH_ARM 2
#define LS_ARCH_ARM64 3
#define LS_ARCH_X86 4
#define LS_ARCH_IA64 5

#define LS_BATTERY_UNKNOWN 0
#define LS_BATTERY_NO_BATTERY 1
#define LS_BATTERY_CHARGING 2
#define LS_BATTERY_DISCHARGING 3

struct ls_meminfo
{
    uint64_t total; // total physical memory in bytes
    uint64_t avail; // available memory in bytes
};

struct ls_cpuinfo
{
    int arch; // LS_ARCH_*
    int num_cores; // physical cores
};

struct ls_batteryinfo
{
    int status; // LS_BATTERY_*
    double charge; // percent
};

//! \brief Stores information about system performance
struct ls_sysmetrics
{
    double cpu_usage; // total cpu utilization, in percent
    uint64_t mem_usage; // process memory usage in bytes

    double gpu_usage; // total gpu utilization, in percent
    uint64_t vram_usage; // process video memory usage in bytes
};

//! \brief Get system memory information
//! 
//! \param mi Structure to store memory system information, cannot
//! be NULL
void ls_get_meminfo(struct ls_meminfo *mi);

//! \brief Get CPU information
//! 
//! \param ci Structure to store CPU information, cannot be NULL
void ls_get_cpuinfo(struct ls_cpuinfo *ci);

//! \brief Get battery information
//! 
//! \param bi Structure to store battery information, cannot be NULL
void ls_get_batteryinfo(struct ls_batteryinfo *bi);

//! \brief Create a performance monitor
//!
//! A performance monitor is an object capable of querying the
//! utilization of system resources. Use ls_query_perf_monitor
//! to collect data.
//!
//! \return A handle to the performance monitor, or NULL on
//! failure.
ls_handle ls_create_perf_monitor(void);

//! \brief Collect performance data
//!
//! Queries the utilization of system resources and populates the
//! data pointed to by sm with the information. If the function
//! fails, the contents of sm are zeroed.
//!
//! Note that you should wait some time in between calling this
//! function, as the performance monitor needs time to collect
//! data.
//!
//! The successful collection of GPU data is not required for the
//! function to succeed (as devices may not have them). Check the
//! vram_avail member in sm to determine whether GPU data was
//! collected.
//!
//! \param mh A handle to a performance monitor
//! \param sm The structure to store the performance information
//!
//! \return 0 on success, -1 on failure
int ls_query_perf_monitor(ls_handle mh, struct ls_sysmetrics *sm);

#endif // _LS_INFO_H_
