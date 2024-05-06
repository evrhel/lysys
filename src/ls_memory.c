#include <lysys/ls_memory.h>

#include "ls_native.h"

size_t ls_page_size(void)
{
#if LS_WINDOWS
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return si.dwPageSize;
#else
	return getpagesize();
#endif // LS_WINDOWS
}

int ls_protect(void *ptr, size_t size, int protect)
{
#if LS_WINDOWS
	BOOL bRet;
	DWORD dwOld;

	bRet = VirtualProtect(ptr, size, ls_protect_to_flags(protect), &dwOld);
	if (!bRet)
		return ls_set_errno_win32(GetLastError());
	return 0;
#else
	int prot;
	int rc;

	prot = ls_protect_to_flags(protect);

	rc = mprotect(ptr, size, prot);
	if (rc == -1)
		return ls_set_errno(ls_errno_to_error(errno));
	return 0;
#endif // LS_WINDOWS
}
