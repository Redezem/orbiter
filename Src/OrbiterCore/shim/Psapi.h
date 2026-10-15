#include <windows.h>
#include <dderr.h>

// Process memory counters referenced by Memstat.h (diagnostics only).
#ifndef _PROCESS_MEMORY_COUNTERS_DEFINED
#define _PROCESS_MEMORY_COUNTERS_DEFINED
struct PROCESS_MEMORY_COUNTERS {
	DWORD cb;
	DWORD PageFaultCount;
	size_t PeakWorkingSetSize;
	size_t WorkingSetSize;
	size_t QuotaPeakPagedPoolUsage;
	size_t QuotaPagedPoolUsage;
	size_t QuotaPeakNonPagedPoolUsage;
	size_t QuotaNonPagedPoolUsage;
	size_t PagefileUsage;
	size_t PeakPagefileUsage;
};
using PPROCESS_MEMORY_COUNTERS = PROCESS_MEMORY_COUNTERS *;
#endif
