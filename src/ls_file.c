#include <lysys/ls_file.h>

#include <lysys/ls_core.h>
#include <lysys/ls_memory.h>
#include <lysys/ls_shell.h>
#include <lysys/ls_stat.h>

#include <stdlib.h>
#include <stdio.h>

#include "ls_handle.h"
#include "ls_native.h"

static void LS_CLASS_FN file_dtor(void *param)
{
#if LS_WINDOWS
	LPHANDLE phFile = param;
	CloseHandle(*phFile);
#else
	close(*(int *)param);
#endif // LS_WINDOWS
}

static const struct ls_class FileClass = {
	.type = LS_FILE,
#if LS_WINDOWS
	.cb = sizeof(HANDLE),
#else
	.cb = sizeof(int),
#endif // LS_WINDOWS
	.dtor = &file_dtor,
	.wait = NULL
};

ls_handle ls_open(const char *path, int access, int share, int create)
{
#if LS_WINDOWS
	PHANDLE phFile;
	HANDLE hFile;
	DWORD dwDesiredAccess;
	DWORD dwFlagsAndAttributes;
	WCHAR szPath[MAX_PATH];
	int len;

	dwDesiredAccess = ls_get_access_rights(access);
	dwFlagsAndAttributes = ls_get_flags_and_attributes(create);

	len = ls_utf8_to_wchar_buf(path, szPath, MAX_PATH);
	if (!len) return NULL;

	phFile = ls_handle_create(&FileClass);
	if (!phFile) return NULL;

	hFile = CreateFileW(szPath, dwDesiredAccess, share, NULL, create,
		dwFlagsAndAttributes, NULL);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		ls_handle_dealloc(phFile);
		return NULL;
	}

	*phFile = hFile;
	return phFile;
#else
	int *pfd;
	int fd;
	int oflags;

	oflags = ls_access_to_oflags(access);
	oflags |= ls_create_to_oflags(create);

	pfd = ls_handle_create(&FileClass);
	if (!pfd) return NULL;

	fd = open(path, oflags, 0666);
	if (fd == -1)
	{
		ls_handle_dealloc(pfd);
		return NULL;
	}

	*pfd = fd;
	return pfd;
#endif // LS_WINDOWS
}

int64_t ls_seek(ls_handle file, int64_t offset, int origin)
{
#if LS_WINDOWS
	BOOL bRet;
	LARGE_INTEGER liDist = { .QuadPart = offset };
	LARGE_INTEGER liNewPointer;
	HANDLE hFile = *(PHANDLE)file;
		
	bRet = SetFilePointerEx(hFile, liDist, &liNewPointer, origin);
	if (!bRet)
		return -1;

	return liNewPointer.QuadPart;
#else
	return lseek(*(int *)file, offset, origin);
#endif // LS_WINDOWS
}

size_t ls_read(ls_handle file, void *buffer, size_t size,
	struct ls_async_io *async)
{
#if LS_WINDOWS
	BOOL bRet;
	DWORD dwRead, dwToRead;
	size_t remaining;
	LPOVERLAPPED lpOverlapped;
	HANDLE hFile = *(PHANDLE)file;

	remaining = size;
	dwToRead = (DWORD)(remaining & 0xffffffff);

	if (async)
	{
		lpOverlapped = (LPOVERLAPPED)async->reserved;
		if (async->event)
			lpOverlapped->hEvent = async->event;

		lpOverlapped->Offset = (DWORD)(async->offset & 0xffffffff);
		lpOverlapped->OffsetHigh = (DWORD)(async->offset >> 32);

		bRet = ReadFile(hFile, buffer, dwToRead, NULL, lpOverlapped);
		if (!bRet)
		{
			if (GetLastError() != ERROR_IO_PENDING)
				return -1;
		}

		return dwToRead;
	}

	while (remaining != 0)
	{
		
		bRet = ReadFile(hFile, buffer, dwToRead, &dwRead, NULL);
		if (!bRet)
			return -1;

		if (dwRead == 0)
			break; // EOF

		remaining -= dwRead;
		dwToRead = (DWORD)(remaining & 0xffffffff);
		buffer = (uint8_t *)buffer + dwRead;
	}

	return size - remaining;
#else
	int fd;
	size_t bytes_read;
	size_t remaining;

	fd = *(int *)file;

	if (async)
		return -1; // TODO: Implement async I/O

	remaining = size;
	while (remaining != 0)
	{
		bytes_read = (size_t)read(fd, buffer, remaining);
		if (bytes_read == -1)
		{
			if (errno == EAGAIN)
				continue;
			return -1;
		}

		if (bytes_read == 0)
			break;

		remaining -= bytes_read;
		buffer = (uint8_t *)buffer + bytes_read;
	}

	return size - remaining;
#endif // LS_WINDOWS
}

size_t ls_write(ls_handle file, const void *buffer, size_t size,
	struct ls_async_io *async)
{
#if LS_WINDOWS
	BOOL bRet;
	DWORD dwWritten, dwToWrite;
	size_t remaining;
	LPOVERLAPPED lpOl;
	HANDLE hFile = *(PHANDLE)file;

	remaining = size;
	dwToWrite = (DWORD)(remaining & 0xffffffff);

	if (async)
	{
		lpOl = (LPOVERLAPPED)async->reserved;
		if (async->event)
			lpOl->hEvent = async->event;

		lpOl->Offset = (DWORD)(async->offset & 0xffffffff);
		lpOl->OffsetHigh = (DWORD)(async->offset >> 32);
		
		bRet = WriteFile(hFile, buffer, dwToWrite, NULL, lpOl);
		if (!bRet)
		{
			if (GetLastError() != ERROR_IO_PENDING)
				return -1;
		}

		return dwToWrite;
	}	

	while (remaining != 0)
	{
		bRet = WriteFile(hFile, buffer, dwToWrite, &dwWritten, NULL);
		if (!bRet)
			return -1;

		remaining -= dwWritten;
		dwToWrite = (DWORD)(remaining & 0xffffffff);
		buffer = (const uint8_t *)buffer + dwWritten;

	}

	return size - remaining;
#else
	int fd;
	size_t bytes_written;
	size_t remaining;

	fd = *(int *)file;

	if (async)
		return -1; // TODO: Implement async I/O

	remaining = size;
	while (remaining != 0)
	{
		bytes_written = (size_t)write(fd, buffer, remaining);
		if (bytes_written == -1)
		{
			if (errno == EAGAIN)
				continue;
			return -1;
		}

		if (bytes_written == 0)
			break;

		remaining -= bytes_written;
		buffer = (const uint8_t *)buffer + bytes_written;
	}

	return size - remaining;
#endif // LS_WINDOWS
}

int ls_flush(ls_handle file)
{
#if LS_WINDOWS
	return FlushFileBuffers(*(PHANDLE)file) ? 0 : -1;
#else
	return fsync(*(int *)file);
#endif // LS_WINDOWS
}

int ls_get_async_io_result(ls_handle file, struct ls_async_io *async,
	unsigned long ms)
{
#if LS_WINDOWS
	DWORD dwRet;
	BOOL bRet;
	LPOVERLAPPED lpOl = (LPOVERLAPPED)async->reserved;
	DWORD dwStatus;
	HANDLE hFile = *(PHANDLE)file;
	
	bRet = GetOverlappedResultEx(hFile, lpOl, &dwRet, ms, FALSE);
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
#else
	// TODO: Implement async I/O
	return LS_IO_ERROR;
#endif // LS_WINDOWS
}

int ls_cancel_async_io(ls_handle file, struct ls_async_io *async)
{
#if LS_WINDOWS
	HANDLE hFile = *(PHANDLE)file;
	LPOVERLAPPED lpOl = async ? (LPOVERLAPPED)async->reserved : NULL;
	return CancelIoEx(hFile, lpOl) ? 0 : -1;
#else
	// TODO: Implement async I/O
	return -1;
#endif // LS_WINDOWS
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
#else
	return rename(old_path, new_path);
#endif // LS_WINDOWS
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
#elif LS_DARWIN
    int rc;
    copyfile_state_t s;
    
    s = copyfile_state_alloc();
    if (!s)
        return -1;
    
    rc = copyfile(old_path, new_path, s, COPYFILE_STAT | COPYFILE_DATA);
    
    copyfile_state_free(s);
    
    return rc == 0 ? 0 : -1;
#else
	int src_fd, dst_fd;
	ssize_t r;

	src_fd = open(old_path, O_RDONLY);
	if (src_fd == -1) return -1;

	dst_fd = open(new_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (dst_fd == -1)
	{
		close(src_fd);
		return -1;
	}

	r = sendfile(dst_fd, src_fd, NULL, 0);

	close(dst_fd);
	close(src_fd);

	return r == -1 ? -1 : 0;
#endif // LS_WINDOWS
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
#else
	return unlink(path);
#endif // LS_WINDOWS
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
#else
	int fd;
	int r;

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd == -1) return -1;

	if (size > 0)
	{
		r = ftruncate(fd, size);
		if (r == -1)
		{
			close(fd);
			return -1;
		}
	}

	close(fd);
	return 0;
#endif // LS_WINDOWS
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
#else
	return mkdir(path, 0777);
#endif // LS_WINDOWS
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

