#include <lysys/ls_file.h>

#include <lysys/ls_core.h>
#include <lysys/ls_memory.h>
#include <lysys/ls_shell.h>
#include <lysys/ls_stat.h>

#include <stdlib.h>

#include "ls_handle.h"
#include "ls_native.h"

#if LS_WINDOWS
static struct ls_class FileClass = {
	.type = LS_FILE,
	.cb = sizeof(HANDLE),
	.dtor = (ls_dtor_t)&CloseHandle,
	.wait = NULL
};
#endif

ls_handle ls_open(const char *path, int access, int share, int create)
{
#if LS_WINDOWS
	HANDLE hFile;
	DWORD dwDesiredAccess;
	DWORD dwFlagsAndAttributes;
	WCHAR szPath[MAX_PATH];
	ls_handle file;
	int len;

	dwDesiredAccess = ls_get_access_rights(access);
	dwFlagsAndAttributes = ls_get_flags_and_attributes(create);

	len = ls_utf8_to_wchar_buf(path, szPath, MAX_PATH);
	if (!len) return NULL;

	file = ls_handle_create(&FileClass);
	if (!file) return NULL;

	hFile = CreateFileW(szPath, dwDesiredAccess, share, NULL, create,
		dwFlagsAndAttributes, NULL);

	if (hFile == INVALID_HANDLE_VALUE) return NULL;

	*(PHANDLE)file = hFile;

	return file;
#endif
}

int64_t ls_seek(ls_handle file, int64_t offset, int origin)
{
#if LS_WINDOWS
	BOOL bRet;
	LARGE_INTEGER liDist = { .QuadPart = offset };
	LARGE_INTEGER liNewPointer;
		
	bRet = SetFilePointerEx(file, liDist, &liNewPointer, origin);
	if (!bRet)
		return -1;

	return liNewPointer.QuadPart;
#endif
}

size_t ls_read(ls_handle file, void *buffer, size_t size,
	struct ls_async_io *async)
{
#if LS_WINDOWS
	BOOL bRet;
	DWORD dwRead, dwToRead;
	size_t remaining;
	LPOVERLAPPED lpOverlapped;

	remaining = size;
	dwToRead = (DWORD)(remaining & 0xffffffff);

	if (async)
	{
		lpOverlapped = (LPOVERLAPPED)async->reserved;
		if (async->event)
			lpOverlapped->hEvent = async->event;

		lpOverlapped->Offset = (DWORD)(async->offset & 0xffffffff);
		lpOverlapped->OffsetHigh = (DWORD)(async->offset >> 32);

		bRet = ReadFile(file, buffer, dwToRead, NULL, lpOverlapped);
		if (!bRet)
		{
			if (GetLastError() != ERROR_IO_PENDING)
				return -1;
		}

		return dwToRead;
	}

	while (remaining != 0)
	{
		
		bRet = ReadFile(file, buffer, dwToRead, &dwRead, NULL);
		if (!bRet)
			return -1;

		if (dwRead == 0)
			break; // EOF

		remaining -= dwRead;
		dwToRead = (DWORD)(remaining & 0xffffffff);
		buffer = (uint8_t *)buffer + dwRead;
	}

	return size - remaining;
#endif
}

size_t ls_write(ls_handle file, const void *buffer, size_t size,
	struct ls_async_io *async)
{
#if LS_WINDOWS
	BOOL bRet;
	DWORD dwWritten, dwToWrite;
	size_t remaining;
	LPOVERLAPPED lpOl;

	remaining = size;
	dwToWrite = (DWORD)(remaining & 0xffffffff);

	if (async)
	{
		lpOl = (LPOVERLAPPED)async->reserved;
		if (async->event)
			lpOl->hEvent = async->event;

		lpOl->Offset = (DWORD)(async->offset & 0xffffffff);
		lpOl->OffsetHigh = (DWORD)(async->offset >> 32);
		
		bRet = WriteFile(file, buffer, dwToWrite, NULL, lpOl);
		if (!bRet)
		{
			if (GetLastError() != ERROR_IO_PENDING)
				return -1;
		}

		return dwToWrite;
	}	

	while (remaining != 0)
	{
		bRet = WriteFile(file, buffer, dwToWrite, &dwWritten, NULL);
		if (!bRet)
			return -1;

		remaining -= dwWritten;
		dwToWrite = (DWORD)(remaining & 0xffffffff);
		buffer = (const uint8_t *)buffer + dwWritten;

	}

	return size - remaining;
#endif
}

int ls_flush(ls_handle file)
{
#if LS_WINDOWS
	return FlushFileBuffers(file) ? 0 : -1;
#endif
}

int ls_get_async_io_result(ls_handle file, struct ls_async_io *async,
	unsigned long ms)
{
#if LS_WINDOWS
	DWORD dwRet;
	BOOL bRet;
	LPOVERLAPPED lpOl = (LPOVERLAPPED)async->reserved;
	DWORD dwStatus;
	
	bRet = GetOverlappedResultEx(file, lpOl, &dwRet, ms, FALSE);
	if (!bRet)
	{
		dwStatus = GetLastError();

		if (dwStatus == ERROR_IO_INCOMPLETE)
			return (int)(async->status = LS_IO_PENDING);
		else if (dwStatus == ERROR_OPERATION_ABORTED)
			return (int)(async->status = LS_IO_CANCELED);

		return (int)(async->status = LS_IO_ERROR);
	}

	async->status = LS_IO_COMPLETED;
	async->transferred = dwRet;

	return (int)async->status;
#endif
}

int ls_cancel_async_io(ls_handle file, struct ls_async_io *async)
{
#if LS_WINDOWS
	LPOVERLAPPED lpOl = async ? (LPOVERLAPPED)async->reserved : NULL;
	return CancelIoEx(file, lpOl) ? 0 : -1;
#endif
}

int ls_move(const char *old_path, const char *new_path)
{
#if LS_WINDOWS
	LPWSTR lpOld, lpNew;
	BOOL rc;

	lpOld = ls_utf8_to_wchar(old_path);
	if (!lpOld) return -1;

	lpNew = ls_utf8_to_wchar(new_path);
	if (!lpNew)
	{
		ls_free(lpOld);
		return -1;
	}

	rc = MoveFileW(lpOld, lpNew);

	ls_free(lpNew);
	ls_free(lpOld);

	return rc ? 0 : -1;
#endif
}

int ls_copy(const char *old_path, const char *new_path)
{
#if LS_WINDOWS
	LPWSTR lpOld, lpNew;
	BOOL rc;

	lpOld = ls_utf8_to_wchar(old_path);
	if (!lpOld) return -1;

	lpNew = ls_utf8_to_wchar(new_path);
	if (!lpNew)
	{
		ls_free(lpOld);
		return -1;
	}

	rc = CopyFileW(lpOld, lpNew, FALSE);

	ls_free(lpNew);
	ls_free(lpOld);

	return rc ? 0 : -1;
#endif
}

int ls_delete(const char *path)
{
#if LS_WINDOWS
	LPWSTR lpPath;
	BOOL rc;

	lpPath = ls_utf8_to_wchar(path);
	if (!lpPath) return -1;

	rc = DeleteFileW(lpPath);

	ls_free(lpPath);

	return rc ? 0 : -1;
#endif
}

int ls_createfile(const char *path, size_t size)
{
#if LS_WINDOWS
	LPWSTR lpPath;
	HANDLE hFile;
	DWORD r;
	LARGE_INTEGER liSize = { .QuadPart = size };

	lpPath = ls_utf8_to_wchar(path);
	if (!lpPath) return -1;

	hFile = CreateFileW(lpPath, GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	ls_free(lpPath);

	if (hFile == INVALID_HANDLE_VALUE)
		return -1;

	if (size > 0)
	{
		r = SetFilePointer(hFile, liSize.LowPart, &liSize.HighPart, FILE_BEGIN);
		if (r == INVALID_SET_FILE_POINTER)
		{
			CloseHandle(hFile);
			return -1;
		}

		if (!SetEndOfFile(hFile))
		{
			CloseHandle(hFile);
			return -1;
		}
	}

	CloseHandle(hFile);
	return 0;
#endif
}

int ls_createdir(const char *path)
{
#if LS_WINDOWS
	LPWSTR lpPath;
	BOOL rc;

	lpPath = ls_utf8_to_wchar(path);
	if (!lpPath) return -1;

	rc = CreateDirectoryW(lpPath, NULL);

	ls_free(lpPath);

	return rc ? 0 : -1;
#endif
}

int ls_createdirs(const char *path)
{
	char *tmp, *cur;
	struct ls_stat st;
	int rc = -1;

	tmp = ls_strdup(path);
	while ((cur = ls_strdir(tmp)))
	{
		*cur = 0;
		rc = ls_stat(tmp, &st);
		*cur = LS_PATH_SEP, cur++;

		if (rc == -1)
		{
			rc = ls_createdir(tmp);
			if (rc == -1) break;
			continue;
		}
		
		if (st.type != LS_FT_DIR)
		{
			rc = -1;
			break;
		}
	}

	ls_free(tmp);
	return rc;
}

