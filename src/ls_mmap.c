#include <lysys/ls_mmap.h>

#include <lysys/ls_core.h>
#include <lysys/ls_memory.h>

#include <stdlib.h>

#include "ls_handle.h"
#include "ls_native.h"

static void LS_CLASS_FN ls_mmap_dtor(void *map)
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
	.dtor = (ls_dtor_t)&ls_mmap_dtor,
	.wait = NULL
};

void *ls_mmap(ls_handle file, size_t size, size_t offset, int protect, ls_handle *map)
{
#if LS_WINDOWS
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

	hMap = CreateFileMappingW(ls_resolve_file(file), NULL, ls_protect_to_flags(protect), liSize.HighPart, liSize.LowPart, NULL);
	if (!hMap) return NULL;

	lpView = MapViewOfFile(hMap, dwAccess, liOffset.HighPart, liOffset.LowPart, 0);
	if (!lpView)
	{
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
        return NULL;
    
    rc = fstat(fd, &st);
    if (rc != 0)
        return NULL;
    
    if (offset > st.st_size)
        return NULL;
    
    max_size = st.st_size - offset;
    if (size == 0)
        size = max_size;
    else if (size > max_size)
        return NULL;
    
    map_res = ls_handle_create(&FileMappingClass);
    
    prot = ls_protect_to_flags(protect);
    
    if (protect & LS_PROT_WRITECOPY)
        flags |= MAP_PRIVATE;
    else
        flags |= MAP_SHARED;
    
    addr = mmap(NULL, size, protect, flags, fd, offset);
    if (!addr)
    {
        ls_close(map_res);
        return NULL;
    }
    
    *map_res = size;
    
    return addr;
#endif // LS_WINDOWS
}

int ls_munmap(ls_handle map, void *addr)
{
#if LS_WINDOWS
	UnmapViewOfFile(addr);
	ls_close(map);
	return 0;
#else
    if (munmap(addr, *(size_t *)map) != 0)
        return -1;
    ls_close(map);
    return 0;
#endif // LS_WINDOWS
}
