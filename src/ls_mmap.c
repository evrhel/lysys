#include <lysys/ls_mmap.h>

#include <lysys/ls_core.h>
#include <lysys/ls_memory.h>

#include <stdlib.h>

#include "ls_handle.h"
#include "ls_native.h"

static void ls_mmap_dtor(void *map)
{
#if LS_WINDOWS
	CloseHandle(*(PHANDLE)map);
#endif // LS_WINDOWS
}

static const struct ls_class FileMappingClass = {
	.type = LS_FILEMAPPING,
#if LS_WINDOWS
	.cb = sizeof(HANDLE),
#else
	.cb = sizeof(size_t),
#endif // LS_WINDOWS
	.dtor = &ls_mmap_dtor,
	.wait = NULL
};

void *ls_mmap(ls_handle file, size_t size, size_t offset, int protect, ls_handle *map)
{
#if LS_WINDOWS
	HANDLE hFile;
	HANDLE hMap;
	handle_t handle;
	LARGE_INTEGER liSize;
	LARGE_INTEGER liOffset = { .QuadPart = offset };
	LPVOID lpView;
	DWORD dwAccess = 0;

	if (protect & (LS_PROT_READ | LS_PROT_WRITE | LS_PROT_WRITECOPY))
		dwAccess = FILE_MAP_WRITE;
	else if (protect & LS_PROT_READ)
		dwAccess = FILE_MAP_READ;

	if (dwAccess == 0)
		return NULL;

	if (protect & LS_PROT_EXEC)
		dwAccess |= FILE_MAP_EXECUTE;

	if (protect & LS_PROT_WRITECOPY)
		dwAccess |= FILE_MAP_COPY;
	
	if (size == 0)
		liSize.QuadPart = 0;
	else
		liSize.QuadPart = size + offset;

	hFile = ls_resolve_file(file);
	if (!hFile)
		return NULL;

	hMap = CreateFileMappingW(hFile, NULL, ls_protect_to_flags(protect), liSize.HighPart, liSize.LowPart, NULL);
	if (!hMap)
	{
		ls_set_errno_win32(GetLastError());
		return NULL;
	}

	lpView = MapViewOfFile(hMap, dwAccess, liOffset.HighPart, liOffset.LowPart, 0);
	if (!lpView)
	{
		ls_set_errno_win32(GetLastError());
		CloseHandle(hMap);
		return NULL;
	}

	handle = ls_handle_create(&FileMappingClass);
	if (!handle)
	{
		UnmapViewOfFile(lpView);
		CloseHandle(hMap);
		return NULL;
	}

	*(PHANDLE)handle = hMap;
	*map = handle;

	return lpView;
#else
	struct stat st;
	int rc;
	void *addr;
	size_t max_size;
	int prot;
	int flags = 0;
	int fd;
	size_t *map_res;

	fd = ls_resolve_file(file);
	
	if (!map)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}
	
	rc = fstat(fd, &st);
	if (rc != 0)
	{
		ls_set_errno(ls_errno_to_error(errno));
		return NULL;
	}
	
	if (offset > st.st_size)
	{
		ls_set_errno(LS_OUT_OF_RANGE);
		return NULL;
	}
	
	max_size = st.st_size - offset;
	if (size == 0)
		size = max_size;
	else if (size > max_size)
		return NULL;
	
	map_res = ls_handle_create(&FileMappingClass);
	if (!map_res)
		return NULL;
	
	prot = ls_protect_to_flags(protect);
	
	if (protect & LS_PROT_WRITECOPY)
		flags |= MAP_PRIVATE;
	else
		flags |= MAP_SHARED;
	
	addr = mmap(NULL, size, protect, flags, fd, offset);
	if (!addr)
	{
		ls_set_errno(ls_errno_to_error(errno));
		ls_handle_dealloc(map_res);
		return NULL;
	}
	
	*map_res = size;
	
	return addr;
#endif // LS_WINDOWS
}

int ls_munmap(ls_handle map, void *addr)
{
#if LS_WINDOWS
	if (!map)
		return ls_set_errno(LS_INVALID_HANDLE);
	if (!addr)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	if (!UnmapViewOfFile(addr))
		return ls_set_errno_win32(GetLastError());

	ls_close(map);
	return 0;
#else
	if (!map)
		return ls_set_errno(LS_INVALID_HANDLE);

	if (!addr)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	if (munmap(addr, *(size_t *)map) == -1)
		return ls_set_errno(ls_errno_to_error(errno));

	ls_close(map);
	return 0;
#endif // LS_WINDOWS
}
