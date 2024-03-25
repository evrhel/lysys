#include <lysys/ls_memory.h>

#include "ls_native.h"

size_t ls_page_size(void)
{
#if LS_WINDOWS
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return si.dwPageSize;
#endif
}

int ls_protect(void *ptr, size_t size, int protect, int *old)
{
#if LS_WINDOWS
	BOOL bRet;
	DWORD dwOld;

	bRet = VirtualProtect(ptr, size, ls_protect_to_flags(protect), &dwOld);
	if (!bRet) return -1;

	if (old)
		*old = ls_flags_to_protect(dwOld);
	return 0;
#endif
}
