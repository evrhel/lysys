#include <lysys/ls_sysinfo.h>

#include <string.h>
#include <stdarg.h>

#include <lysys/ls_core.h>

#include "ls_handle.h"
#include "ls_native.h"

void ls_get_meminfo(struct ls_meminfo *mi)
{
#if LS_WINDOWS
	MEMORYSTATUSEX ms;

	ms.dwLength = sizeof(ms);
	if (!GlobalMemoryStatusEx(&ms))
	{
		mi->total = 0;
		mi->avail = 0;
		return;
	}

	mi->total = ms.ullTotalPhys;
	mi->avail = ms.ullAvailPhys;
#elif LS_DARWIN
	struct xsw_usage xsw;
	size_t len;
	int rc;

	mi->total = 0;
	mi->avail = 0;

	len = sizeof(xsw);
	rc = sysctlbyname("vm.loadavg", &xsw, &len, NULL, 0);
	if (rc == -1)
	{
		mi->total = 0;
		mi->avail = 0;
		return;
	}

	mi->total = xsw.xsu_total;
	mi->avail = xsw.xsu_avail;
#else
	struct sysinfo si;
	int rc;

	rc = sysinfo(&si);
	if (rc == -1)
	{
		mi->total = 0;
		mi->avail = 0;
		return;
	}

	mi->total = si.totalram;
	mi->avail = si.freeram;
#endif // LS_WINDOWS
}

void ls_get_cpuinfo(struct ls_cpuinfo *ci)
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
#else
	struct utsname uts;
	long rc;

	rc = uname(&uts);
	if (rc == -1)
	{
		ci->num_cores = 0;
		ci->arch = LS_ARCH_UNKNOWN;
		return;
	}

	ci->num_cores = (int)sysconf(_SC_NPROCESSORS_ONLN);

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
#endif // LS_WINDOWS
}

void ls_get_batteryinfo(struct ls_batteryinfo *bi)
{
#if LS_WINDOWS
	SYSTEM_POWER_STATUS sps;
	BOOL b;

	b = GetSystemPowerStatus(&sps);
	if (!b)
	{
		bi->status = LS_BATTERY_UNKNOWN;
		bi->charge = 0;
		return;
	}

	if (sps.BatteryFlag & 128)
	{
		bi->status = LS_BATTERY_NO_BATTERY;
		bi->charge = 0;
	}
	else if (sps.ACLineStatus == 0)
	{
		bi->status = LS_BATTERY_DISCHARGING;
		bi->charge = (double)sps.BatteryLifePercent;
	}
	else if (sps.ACLineStatus == 1)
	{
		bi->status = LS_BATTERY_CHARGING;
		bi->charge = (double)sps.BatteryLifePercent;
	}
	else
	{
		bi->status = LS_BATTERY_UNKNOWN;
		bi->charge = 0;
	}
#elif LS_DARWIN
	int rc;
	CFTypeRef blob;
	CFArrayRef sources;
	CFDictionaryRef desc;
	CFNumberRef capacity;
	CFNumberRef current_capacity;
	CFBooleanRef is_charging;
	int32_t cur_charge;
	int32_t max_charge;
	
	blob = IOPSCopyPowerSourcesInfo();
	if (!blob)
	{
		bi->status = LS_BATTERY_UNKNOWN;
		bi->charge = 0;
		return;
	}
	
	sources = IOPSCopyPowerSourcesList(blob);
	
	if (!sources)
	{
		CFRelease(sources);
		bi->status = LS_BATTERY_UNKNOWN;
		bi->charge = 0;
		return;
	}
	
	desc = IOPSGetPowerSourceDescription(blob, CFArrayGetValueAtIndex(sources, 0));
	if (!desc)
		goto no_battery;
	
	capacity = CFDictionaryGetValue(desc, CFSTR(kIOPSMaxCapacityKey));
	if (!capacity)
		goto no_battery;
	CFNumberGetValue(capacity, kCFNumberSInt32Type, &max_charge);
	
	current_capacity = CFDictionaryGetValue(desc, CFSTR(kIOPSCurrentCapacityKey));
	if (!current_capacity)
		goto no_battery;
	CFNumberGetValue(current_capacity, kCFNumberSInt32Type, &cur_charge);
	
	is_charging = CFDictionaryGetValue(desc, CFSTR(kIOPSIsChargingKey));
	if (!is_charging)
		goto no_battery;
	
	bi->status = CFBooleanGetValue(is_charging) ? LS_BATTERY_CHARGING : LS_BATTERY_DISCHARGING;
	bi->charge = 100.0 * cur_charge / max_charge;
	
	return;
no_battery:
	CFRelease(sources);
	CFRelease(blob);
	
	bi->status = LS_BATTERY_NO_BATTERY;
	bi->charge = 0;
#else
	bi->status = LS_BATTERY_UNKNOWN;
	bi->charge = 0;
#endif // LS_WINDOWS
}

struct ls_perf_monitor
{
#if LS_WINDOWS
	PDH_HQUERY query;
	PDH_HCOUNTER cpu_usage_counter;
	PDH_HCOUNTER gpu_usage_counter;
	PDH_HCOUNTER gpu_vram_usage_counter;
#elif LS_DARWIN
	mach_port_t master;
	struct host_cpu_load_info prev_cpu;
#else
#endif // LS_WINDOWS
};

static void ls_perf_monitor_dtor(struct ls_perf_monitor *m)
{
#if LS_WINDOWS
	(void)PdhCloseQuery(m->query);
#elif LS_DARWIN
	mach_port_deallocate(mach_task_self(), m->master);
#else
#endif // LS_WINDOWS
}

static const struct ls_class PerfMonitor = {
	.type = LS_PERF_MONITOR,
	.cb = sizeof(struct ls_perf_monitor),
	.dtor = (ls_dtor_t)&ls_perf_monitor_dtor,
	.wait = NULL
};

#if LS_WINDOWS

static WCHAR *ls_search_for_instance(PZZWSTR mszCounterList, const WCHAR *szInstance)
{
	PZZWSTR mszPtr;
	for (mszPtr = mszCounterList; *mszPtr != 0; mszPtr += wcslen(mszPtr) + 1)
	{
		if (!ls_match_string(mszPtr, szInstance)) 
			return mszPtr;
	}

	ls_set_errno(LS_NOT_FOUND);
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
		return ls_set_errno_pdh(rc);

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
	ls_free(mszInstanceList);

	if (rc != ERROR_SUCCESS)
	{
		ls_free(mszCounterList);
		return ls_set_errno_pdh(rc);
	}

	szObjectInst = ls_search_for_instance(mszCounterList, szInstance);
	if (!szObjectInst)
	{
		ls_free(mszCounterList);
		return -1;
	}

	hr = StringCbPrintfW(aBuffer, sizeof(aBuffer), L"\\%s(%s)\\%s",
		szObject, szObjectInst, szCounter);
	if (!SUCCEEDED(hr))
	{
		ls_set_errno_hresult(hr);
		ls_free(mszCounterList);
		return -1;
	}

	rc = PdhAddCounterW(hQuery, aBuffer, 0, phCounter);
	if (rc != ERROR_SUCCESS)
	{
		ls_set_errno_pdh(rc);
		ls_free(mszCounterList);
		*phCounter = NULL;
		return -1;
	}

	ls_free(mszCounterList);
	return 0;
}

static int ls_add_counterf(HQUERY hQuery, LPCWSTR szObject,
	STRSAFE_LPCWSTR pszInstanceFormat, LPCWSTR szCounter,
	PDH_HCOUNTER *phCounter, ...)
{
	va_list args;
	WCHAR szInstance[256];
	HRESULT hr;

	va_start(args, phCounter);
	hr = StringCbVPrintfW(szInstance, sizeof(szInstance),
		pszInstanceFormat, args);
	va_end(args);

	if (!SUCCEEDED(hr))
		return ls_set_errno_hresult(hr);

	return ls_add_counter(hQuery, szObject, szInstance, szCounter, phCounter);
}

#elif LS_DARWIN

//! \brief Query CPU and DRAM usage
//!
//! \param m A performance monitor
//! \param sm Structure to populate, unmodified on error
//!
//! \return 0 on success, -1 on error
static int ls_query_cpu_darwin(struct ls_perf_monitor *m, struct ls_sysmetrics *sm)
{
	kern_return_t kr;
	mach_msg_type_number_t count;
	struct host_cpu_load_info cpu;
	natural_t user, nice, system, idle;
	natural_t total;
	struct task_basic_info task;
	struct host_basic_info host;
	
	// CPU utilization
	
	count = HOST_CPU_LOAD_INFO_COUNT;
	
	kr = host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&cpu, &count);
	if (kr != KERN_SUCCESS)
		return ls_set_errno_kr(kr);
	
	user = cpu.cpu_ticks[CPU_STATE_USER] - m->prev_cpu.cpu_ticks[CPU_STATE_USER];
	nice = cpu.cpu_ticks[CPU_STATE_NICE] - m->prev_cpu.cpu_ticks[CPU_STATE_NICE];
	system = cpu.cpu_ticks[CPU_STATE_SYSTEM] - m->prev_cpu.cpu_ticks[CPU_STATE_SYSTEM];
	idle = cpu.cpu_ticks[CPU_STATE_IDLE] - m->prev_cpu.cpu_ticks[CPU_STATE_IDLE];
	m->prev_cpu = cpu;
	
	total = user + nice + system + idle;
	
	// DRAM usage
	
	count = TASK_BASIC_INFO_COUNT;
	
	kr = task_info(mach_host_self(), TASK_BASIC_INFO, (task_info_t)&task, &count);
	if (kr != KERN_SUCCESS)
		return ls_set_errno_kr(kr);
	
	count = HOST_BASIC_INFO_COUNT;
	
	kr = host_info(mach_host_self(), HOST_BASIC_INFO, (host_info_t)&host, &count);
	if (kr != KERN_SUCCESS)
		return ls_set_errno(kr);
	
	sm->cpu_usage = (user + nice + system) * 100.0 / total;
	sm->mem_usage = task.resident_size;
	
	return 0;
}

//! \brief Query GPU and VRAM usage
//!
//! \param m A performance monitor
//! \param sm Structure to populate, unmodified on error
//!
//! \return 0 on success, -1 on error
static int ls_query_gpu_darwin(struct ls_perf_monitor *m, struct ls_sysmetrics *sm)
{
	kern_return_t kr;
	CFMutableDictionaryRef matching;
	io_iterator_t it;
	io_registry_entry_t ent;
	CFMutableDictionaryRef service;
	CFMutableDictionaryRef props;
	CFNumberRef utilization;
	CFNumberRef used_vram;
	int64_t usage;
	
	matching = IOServiceMatching("IOAccelerator");
	if (!matching)
		return ls_set_errno(LS_NOT_FOUND);
	
	kr = IOServiceGetMatchingServices(m->master, matching, &it);
	if (kr != kIOReturnSuccess)
		return ls_set_errno_kr(kr);
	
	while ((ent = IOIteratorNext(it)))
	{
		kr = IORegistryEntryCreateCFProperties(ent, &service, NULL, 0);
		if (kr != kIOReturnSuccess)
		{
			IOObjectRelease(it);
			return ls_set_errno(kr);
		}
		
		props = (CFMutableDictionaryRef)CFDictionaryGetValue(service, CFSTR("PerformanceStatistics"));
		if (!props)
		{
			CFRelease(service);
			continue;
		}
		
		utilization = CFDictionaryGetValue(props, CFSTR("GPU Core Utilization"));
		used_vram = CFDictionaryGetValue(props, CFSTR("vramUsedBytes"));
		
		if (!utilization || !used_vram)
		{
			CFRelease(service);
			continue;
		}
		
		CFNumberGetValue(utilization, kCFNumberSInt64Type, &usage);
		CFNumberGetValue(used_vram, kCFNumberSInt64Type, &sm->vram_usage);
		
		sm->gpu_usage = usage / 10000000.0;
		
		CFRelease(service);
		IOObjectRelease(it);
		return 0;
	}
	
	IOObjectRelease(it);
	return ls_set_errno(LS_NOT_FOUND);
}

#else
#endif // LS_WINDOWS

ls_handle ls_create_perf_monitor(void)
{
#if LS_WINDOWS
	struct ls_perf_monitor *m;
	PDH_STATUS rc;
	DWORD dwPid;

	m = ls_handle_create(&PerfMonitor);
	if (!m)
		return NULL;

	// Collect information using PDH
	// Use perfmon.exe to find the counter names

	rc = PdhOpenQueryW(NULL, 0, &m->query);
	if (rc != ERROR_SUCCESS)
	{
		ls_set_errno_pdh(rc);
		ls_handle_dealloc(m);
		return NULL;
	}

	dwPid = GetCurrentProcessId();

	// CPU usage counter
	rc = PdhAddCounterW(
			m->query,
			L"\\Processor Information(_Total)\\% Processor Time",
			0,
			&m->cpu_usage_counter);
	if (rc != ERROR_SUCCESS) // only cpu usage is mandatory
	{
		ls_set_errno_pdh(rc);
		PdhCloseQuery(m->query);
		ls_handle_dealloc(m);
		return NULL;
	}

	// GPU usage counter
	(void)ls_add_counterf(m->query, L"GPU Engine", L"pid_%d_*engtype_3D",
		L"Utilization Percentage", &m->gpu_usage_counter, dwPid);

	// GPU VRAM usage counter
	(void)ls_add_counterf(m->query, L"GPU Process Memory", L"pid_%d*",
		L"Dedicated Usage", &m->gpu_vram_usage_counter, dwPid);

	// collect once to initialize the counters
	(void)PdhCollectQueryData(m->query);

	return m;
#elif LS_DARWIN
	kern_return_t kr;
	struct ls_perf_monitor *m;
	
	m = ls_handle_create(&PerfMonitor);
	if (!m)
		return NULL;
	
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

	// deprecated on macOS 12.0
	// need for compatability with macOS 10.13
	kr = IOMasterPort(0, &m->master);
	if (kr != KERN_SUCCESS)
	{
		ls_handle_dealloc(m);
		ls_set_errno_kr(kr);
		return NULL;
	}

#pragma clang diagnostic pop
	
	return m;
#else
	ls_set_errno(LS_NOT_IMPLEMENTED);
	return NULL;
#endif // LS_WINDOWS
}

int ls_query_perf_monitor(ls_handle mh, struct ls_sysmetrics *sm)
{
#if LS_WINDOWS
	struct ls_perf_monitor *m = mh;
	PDH_FMT_COUNTERVALUE pdhCpuValue, pdhGpuValue, pdhVramUsageValue;
	PROCESS_MEMORY_COUNTERS pmc;
	BOOL b;
	PDH_STATUS ps;

	if (!mh)
		return ls_set_errno(LS_INVALID_HANDLE);
	if (!sm)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	// Collect data from the query

	ps = PdhCollectQueryData(m->query);
	if (ps != ERROR_SUCCESS)
		return ls_set_errno_pdh(ps);

	if (m->cpu_usage_counter)
	{
		ps = PdhGetFormattedCounterValue(m->cpu_usage_counter,
			PDH_FMT_LONG, NULL, &pdhCpuValue);

		if (ps != ERROR_SUCCESS)
			return ls_set_errno_pdh(ps);
	}
	else
		pdhCpuValue.longValue = 0;

	if (m->gpu_usage_counter)
	{
		ps = PdhGetFormattedCounterValue(m->gpu_usage_counter,
			PDH_FMT_LONG, NULL, &pdhGpuValue);

		if (ps != ERROR_SUCCESS)
			return ls_set_errno_pdh(ps);
	}
	else
		pdhGpuValue.longValue = 0;

	if (m->gpu_vram_usage_counter)
	{
		ps = PdhGetFormattedCounterValue(m->gpu_vram_usage_counter,
			PDH_FMT_LONG, NULL, &pdhVramUsageValue);

		if (ps != ERROR_SUCCESS)
			return ls_set_errno_pdh(ps);
	}
	else
		pdhVramUsageValue.longValue = 0;

	b = GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
	if (!b)
		return ls_set_errno_pdh(ps);

	// Fill the structure

	sm->cpu_usage = pdhCpuValue.longValue;
	sm->mem_usage = pmc.WorkingSetSize;

	sm->gpu_usage = pdhGpuValue.longValue;
	sm->vram_usage = pdhVramUsageValue.longValue;

	return 0;
#elif LS_DARWIN
	struct ls_perf_monitor *m;
	int rc;
	
	if (!mh)
		return ls_set_errno(LS_INVALID_HANDLE);
	if (!sm)
		return ls_set_errno(LS_INVALID_ARGUMENT);
	
	m = mh;
	
	rc = ls_query_cpu_darwin(m, sm);
	if (rc == -1)
		return -1;
	
	(void)ls_query_gpu_darwin(m, sm);
	
	return 0;
#else
	return ls_set_errno(LS_NOT_IMPLEMENTED);
#endif // LS_WINDOWS
}
