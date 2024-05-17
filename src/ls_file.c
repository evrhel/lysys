#include <lysys/ls_file.h>

#include <lysys/ls_core.h>
#include <lysys/ls_memory.h>
#include <lysys/ls_shell.h>
#include <lysys/ls_stat.h>
#include <lysys/ls_string.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ls_handle.h"
#include "ls_native.h"
#include "ls_sync_util.h"
#include "ls_util.h"

#if LS_WINDOWS
#define PIPE_BUF_SIZE 4096
#define PIPE_MODE (PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT)
#endif // LS_WINDOWS

static void ls_file_dtor(void *param)
{
#if LS_WINDOWS
	CloseHandle(*(LPHANDLE)param);
#else
	(void)close(*(int *)param);
#endif // LS_WINDOWS
}

static const struct ls_class FileClass = {
	.type = LS_FILE,
#if LS_WINDOWS
	.cb = sizeof(HANDLE),
#else
	.cb = sizeof(int),
#endif // LS_WINDOWS
	.dtor = &ls_file_dtor,
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

	if (ls_utf8_to_wchar_buf(path, szPath, MAX_PATH) == -1)
		return NULL;

	phFile = ls_handle_create(&FileClass);
	if (!phFile)
		return NULL;

	dwDesiredAccess = ls_get_access_rights(access);
	dwFlagsAndAttributes = ls_get_flags_and_attributes(access);

	hFile = CreateFileW(szPath, dwDesiredAccess, share, NULL, create,
		dwFlagsAndAttributes, NULL);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		ls_set_errno_win32(GetLastError());
		ls_handle_dealloc(phFile);
		return NULL;
	}

	*phFile = hFile;
	return phFile;
#else
	int *pfd;
	int fd;
	int oflags;

	if (!path)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	pfd = ls_handle_create(&FileClass);
	if (!pfd)
		return NULL;

	oflags = ls_access_to_oflags(access);
	oflags |= ls_create_to_oflags(create);

	fd = open(path, oflags, 0666);
	if (fd == -1)
	{
		ls_set_errno(ls_errno_to_error(errno));
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
	HANDLE hFile;

	hFile = ls_resolve_file(file);
	if (!hFile)
		return -1;

	bRet = SetFilePointerEx(hFile, liDist, &liNewPointer, origin);
	if (!bRet)
		return ls_set_errno_win32(GetLastError());
	return liNewPointer.QuadPart;
#else
	int fd;
	off_t r;

	fd = ls_resolve_file(file);
	if (fd == -1)
		return -1;

	r = lseek(fd, offset, origin);
	if (r == -1)
		return ls_set_errno(ls_errno_to_error((int)r));
	return r;
#endif // LS_WINDOWS
}

size_t ls_read(ls_handle fh, void *buffer, size_t size)
{
#if LS_WINDOWS
	BOOL bRet;
	DWORD dwRead, dwToRead;
	size_t remaining;
	HANDLE hFile;
	
	hFile = ls_resolve_file(fh);
	if (!hFile)
		return -1;

	remaining = size;
	dwToRead = (DWORD)(remaining & 0xffffffff);

	while (remaining != 0)
	{
		bRet = ReadFile(hFile, buffer, dwToRead, &dwRead, NULL);
		if (!bRet)
			return ls_set_errno_win32(GetLastError());

		if (dwRead == 0)
			break; // EOF

		remaining -= dwRead;
		dwToRead = (DWORD)(remaining & 0xffffffff);
		buffer = (uint8_t *)buffer + dwRead; // advance buffer
	}

	return size - remaining;
#else
	int fd;
	size_t bytes_read;
	size_t remaining;

	fd = ls_resolve_file(fh);
	if (fd == -1)
		return -1;

	remaining = size;
	while (remaining != 0)
	{
		bytes_read = (size_t)read(fd, buffer, remaining);
		if (bytes_read == -1)
		{
			if (errno == EAGAIN)
				continue;
			return ls_set_errno(ls_errno_to_error(errno));
		}

		if (bytes_read == 0)
			break;

		remaining -= bytes_read;
		buffer = (uint8_t *)buffer + bytes_read; // advance buffer
	}

	return size - remaining;
#endif // LS_WINDOWS
}

size_t ls_write(ls_handle fh, const void *buffer, size_t size)
{
#if LS_WINDOWS
	BOOL bRet;
	DWORD dwWritten, dwToWrite;
	size_t remaining;
	HANDLE hFile;
	
	hFile = ls_resolve_file(fh);
	if (!hFile)
		return -1;

	remaining = size;
	dwToWrite = (DWORD)(remaining & 0xffffffff);

	while (remaining != 0)
	{
		bRet = WriteFile(hFile, buffer, dwToWrite, &dwWritten, NULL);
		if (!bRet)
			return ls_set_errno_win32(GetLastError());

		remaining -= dwWritten;
		dwToWrite = (DWORD)(remaining & 0xffffffff);
		buffer = (const uint8_t *)buffer + dwWritten; // advance buffer

	}

	return size - remaining;
#else
	int fd;
	size_t bytes_written;
	size_t remaining;

	fd = ls_resolve_file(fh);

	remaining = size;
	while (remaining != 0)
	{
		bytes_written = (size_t)write(fd, buffer, remaining);
		if (bytes_written == -1)
		{
			if (errno == EAGAIN)
				continue;
			return ls_set_errno(ls_errno_to_error(errno));
		}

		if (bytes_written == 0)
			break;

		remaining -= bytes_written;
		buffer = (const uint8_t *)buffer + bytes_written; // advance buffer
	}

	return size - remaining;
#endif // LS_WINDOWS
}

int ls_flush(ls_handle file)
{
#if LS_WINDOWS
	BOOL b;
	HANDLE hFile;

	hFile = ls_resolve_file(file);
	if (!hFile)
		return -1;

	b = FlushFileBuffers(hFile);
	if (!b)
		return ls_set_errno_win32(GetLastError());
	return 0;
#else
	int fd;
	int rc;

	fd = ls_resolve_file(file);
	if (fd == -1)
		return -1;

	rc = fsync(fd);
	if (rc == -1)
		return ls_set_errno(ls_errno_to_error(errno));
	return 0;
#endif // LS_WINDOWS
}

struct ls_aio
{
#if LS_WINDOWS
	OVERLAPPED ol;
	HANDLE hFile;
#else
	struct aiocb aiocb;
	ls_lock_t lock;
	ls_cond_t cond;
	ssize_t bytes_transferred; // -1 = no transfers
	int status;
	int error;
#endif // LS_WINDOWS
};

#if LS_POSIX

//! \brief Update the status and notify waiting threads if necessary
//!
//! If the status will not change, the function returns immediately.
//!
//! If the status is set to LS_AIO_ERROR, set aio->error to errno
//! or an equivalent POSIX error code.
//!
//! aio->lock must be held by the calling thread.
//!
//! \param aio Asynchronous I/O object
//! \param status New status
static void ls_aio_update_status(struct ls_aio *aio, int status)
{
	if (aio->status == status)
		return;

	aio->status = status;
	aio->bytes_transferred = -1;

	if (status == LS_AIO_COMPLETED)
	{
		aio->bytes_transferred = aio_return(&aio->aiocb);
		if (aio->bytes_transferred == -1)
		{
			aio->error = errno;
			aio->status = LS_AIO_ERROR;
		}
	}
	
	cond_broadcast(&aio->cond);
}

//! \brief Check request status and update the status if necessary
//!
//! aio->lock must be held
//!
//! \param aio Asynchronous I/O object
//!
//! \return If the request has completed, 0 is returned, otherwise
//! -1 is returned and ls_errno is set.
static int ls_aio_check_error(struct ls_aio *aio)
{
	int rc;

	rc = aio_error(&aio->aiocb);
	if (rc == 0)
	{
		ls_aio_update_status(aio, LS_AIO_COMPLETED);
		return 0;
	}
	else if (rc > 0)
	{
		aio->error = rc;
		ls_aio_update_status(aio, LS_AIO_ERROR);

		 // aio->error may have been changed by ls_aio_update_status
		return ls_set_errno(ls_errno_to_error(aio->error));
	}
	else if (rc == EINPROGRESS)
	{
		ls_aio_update_status(aio, LS_AIO_PENDING);
		return ls_set_errno(LS_BUSY);
	}
	else if (rc == ECANCELED)
	{
		ls_aio_update_status(aio, LS_AIO_CANCELED);
		return ls_set_errno(LS_CANCELED);
	}
	
	// see aio_error(3)
	__builtin_unreachable();
}

static void ls_aio_handler(union sigval sigval)
{
	struct ls_aio *aio = sigval.sival_ptr;

	lock_lock(&aio->lock);

	(void)ls_aio_check_error(aio);
	
	lock_unlock(&aio->lock);
}

#endif // LS_POSIX

static void ls_aio_dtor(struct ls_aio *aio)
{
#if LS_WINDOWS
	CloseHandle(aio->ol.hEvent);
#else
#endif // LS_WINDOWS
}

static int ls_aio_wait(struct ls_aio *aio, unsigned long ms)
{
#if LS_WINDOWS
	DWORD dwRet;

	dwRet = WaitForSingleObject(aio->ol.hEvent, ms);

	if (dwRet == WAIT_OBJECT_0)
		return 0;

	if (dwRet == WAIT_TIMEOUT)
		return 1;

	return ls_set_errno_win32(GetLastError());
#else
	int rc;
	struct timespec ts;

	lock_lock(&aio->lock);
	
	while (aio->status == LS_AIO_PENDING)
	{
		rc = cond_wait(&aio->cond, &aio->lock, ms);
		if (rc == 1)
		{
			lock_unlock(&aio->lock);
			return 1;
		}
	}

	lock_unlock(&aio->lock);

	return 0;
#endif // LS_WINDOWS
}

static const struct ls_class AioClass = {
	.type = LS_AIO,
	.cb = sizeof(struct ls_aio),
	.dtor = (ls_dtor_t)&ls_aio_dtor,
	.wait = (ls_wait_t)&ls_aio_wait
};

ls_handle ls_aio_open(ls_handle fh)
{
#if LS_WINDOWS
	struct ls_aio *aio;
	HANDLE hFile;

	hFile = ls_resolve_file(fh);
	if (!hFile)
		return NULL;
	
	aio = ls_handle_create(&AioClass);
	if (!aio)
		return NULL;

	aio->ol.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
	if (!aio->ol.hEvent)
	{
		ls_set_errno_win32(GetLastError());
		ls_handle_dealloc(aio);
		return NULL;
	}

	aio->hFile = hFile;

	return aio;
#else
	struct ls_aio *aio;
	int fd;
	int rc;
	struct sigevent *sev;

	fd = ls_resolve_file(fh);
	if (fd == -1)
		return NULL;

	aio = ls_handle_create(&AioClass);
	if (!aio)
		return NULL;

	rc = lock_init(&aio->lock);
	if (rc == -1)
	{
		ls_handle_dealloc(aio);
		return NULL;
	}

	rc = cond_init(&aio->cond);
	if (rc == -1)
	{
		lock_destroy(&aio->lock);
		ls_handle_dealloc(aio);
		return NULL;
	}

	aio->aiocb.aio_fildes = fd;
	
	// notifaction handler
	sev = &aio->aiocb.aio_sigevent;
	sev->sigev_notify = SIGEV_THREAD;
	sev->sigev_signo = 0;
	sev->sigev_value.sival_ptr = aio;
	sev->sigev_notify_function = &ls_aio_handler;
	sev->sigev_notify_attributes = NULL;

	aio->bytes_transferred = -1;

	return aio;
#endif // LS_WINDOWS
}

int ls_aio_read(ls_handle aioh, uint64_t offset, volatile void *buffer, size_t size)
{
#if LS_WINDOWS
	struct ls_aio *aio;
	BOOL b;
	DWORD dwToRead;
	DWORD dwErr;
	ULARGE_INTEGER uliOffset = { .QuadPart = offset };

	if (!aioh)
		return ls_set_errno(LS_INVALID_HANDLE);

	aio = aioh;

	aio->ol.Offset = uliOffset.LowPart;
	aio->ol.OffsetHigh = uliOffset.HighPart;

	if (!ResetEvent(aio->ol.hEvent))
		return ls_set_errno_win32(GetLastError());

	dwToRead = (DWORD)(size & 0xffffffff);
	b = ReadFile(aio->hFile, (LPVOID)buffer, dwToRead, NULL, &aio->ol);
	if (!b)
	{
		dwErr = GetLastError();
		if (dwErr == ERROR_IO_PENDING)
			return 0;

		ls_set_errno_win32(dwErr);
		SetEvent(aio->ol.hEvent); // prevent deadlock
		return -1;
	}

	return 0;
#else
	struct ls_aio *aio;
	int rc;

	if (!aioh)
		return ls_set_errno(LS_INVALID_HANDLE);

	aio = aioh;

	lock_lock(&aio->lock);

	if (aio->status == LS_AIO_PENDING)
	{
		lock_unlock(&aio->lock);
		return ls_set_errno(LS_BUSY);
	}

	aio->aiocb.aio_offset = offset;
	aio->aiocb.aio_buf = buffer;
	aio->aiocb.aio_nbytes = size;

	rc = aio_read(&aio->aiocb);
	if (rc == -1)
	{
		ls_set_errno(ls_errno_to_error(errno));
		lock_unlock(&aio->lock);
		return -1;
	}

	aio->status = LS_AIO_PENDING;
	
	lock_unlock(&aio->lock);

	return 0;
#endif // LS_WINDOWS
}

int ls_aio_write(ls_handle aioh, uint64_t offset, const volatile void *buffer, size_t size)
{
#if LS_WINDOWS
	struct ls_aio *aio;
	BOOL b;
	DWORD dwToWrite;
	DWORD dwErr;
	ULARGE_INTEGER uliOffset = { .QuadPart = offset };

	if (!aioh)
		return ls_set_errno(LS_INVALID_HANDLE);

	aio = aioh;

	aio->ol.Offset = uliOffset.LowPart;
	aio->ol.OffsetHigh = uliOffset.HighPart;

	if (!ResetEvent(aio->ol.hEvent))
		return ls_set_errno_win32(GetLastError());

	dwToWrite = (DWORD)(size & 0xffffffff);
	b = WriteFile(aio->hFile, (LPCVOID)buffer, dwToWrite, NULL, &aio->ol);
	if (!b)
	{
		dwErr = GetLastError();
		if (dwErr == ERROR_IO_PENDING)
			return 0;

		ls_set_errno_win32(dwErr);
		SetEvent(aio->ol.hEvent); // prevent deadlock
		return -1;
	}

	return 0;
#else
	struct ls_aio *aio;
	int rc;

	if (!aioh)
		return ls_set_errno(LS_INVALID_HANDLE);

	aio = aioh;

	lock_lock(&aio->lock);

	if (aio->status == LS_AIO_PENDING)
	{
		lock_unlock(&aio->lock);
		return ls_set_errno(LS_BUSY);
	}

	aio->aiocb.aio_offset = offset;
	aio->aiocb.aio_buf = (volatile void *)buffer;
	aio->aiocb.aio_nbytes = size;

	rc = aio_write(&aio->aiocb);
	if (rc == -1)
	{
		ls_set_errno(ls_errno_to_error(errno));
		lock_unlock(&aio->lock);
		return -1;
	}

	aio->status = LS_AIO_PENDING;
	
	lock_unlock(&aio->lock);

	return 0;
#endif // LS_WINDOWS
}

int ls_aio_status(ls_handle aioh, size_t *transferred)
{
#if LS_WINDOWS
	struct ls_aio *aio = aioh;
	DWORD dwTransferred;
	BOOL b;
	DWORD dwErr;

	b = GetOverlappedResult(aio->hFile, &aio->ol, &dwTransferred, FALSE);
	if (!b)
	{
		dwErr = GetLastError();

		if (dwErr == ERROR_IO_INCOMPLETE)
			return LS_AIO_PENDING;

		if (dwErr == ERROR_OPERATION_ABORTED)
			return LS_AIO_CANCELED;

		return ls_set_errno_win32(dwErr);
	}

	*transferred = dwTransferred;

	return LS_AIO_COMPLETED;
#else
	struct ls_aio *aio;
	int rc;
	int status;

	if (!aioh)
		return ls_set_errno(LS_INVALID_HANDLE);

	lock_lock(&aio->lock);

	status = aio->status;

	if (status == LS_AIO_COMPLETED)
	{
		if (aio->bytes_transferred == -1)
		{
			// no requests have been made yet
			lock_unlock(&aio->lock);
			return ls_set_errno(LS_NOT_READY);
		}

		*transferred = aio->bytes_transferred;
	}
	else if (status == LS_AIO_ERROR)
	{
		lock_unlock(&aio->lock);
		return ls_set_errno(aio->error);
	}

	lock_unlock(&aio->lock);

	return status;
#endif // LS_WINDOWS
}

int ls_aio_cancel(ls_handle aioh)
{
#if LS_WINDOWS
	struct ls_aio *aio = aioh;
	BOOL b;

	if (!aioh)
		return ls_set_errno(LS_INVALID_HANDLE);

	b = CancelIoEx(aio->hFile, &aio->ol);
	if (!b)
		return ls_set_errno_win32(GetLastError());

	return 0;
#else
	struct ls_aio *aio;
	int rc;

	if (!aioh)
		return ls_set_errno(LS_INVALID_HANDLE);

	aio = aioh;

	lock_lock(&aio->lock);

	rc = aio_cancel(aio->aiocb.aio_fildes, &aio->aiocb);
	if (rc == -1)
	{
		ls_set_errno(ls_errno_to_error(errno));
		lock_unlock(&aio->lock);
		return -1;
	}

	if (rc == AIO_CANCELED)
	{
		ls_aio_update_status(aio, LS_AIO_CANCELED);
		lock_unlock(&aio->lock);
		return 0;
	}

	if (rc == AIO_ALLDONE)
	{
		ls_aio_update_status(aio, LS_AIO_COMPLETED);
		lock_unlock(&aio->lock);
		return 0;
	}

	if (rc == AIO_NOTCANCELED)
	{
		rc = ls_aio_check_error(aio);
		lock_unlock(&aio->lock);

		if (rc == -1)
		{
			if (_ls_errno == LS_CANCELED)
				return 0;
			return -1;
		}

		// completed
		return 0;
	}

	// see aio_cancel(3), aio_error(3)
	__builtin_unreachable();
#endif // LS_WINDOWS
}

int ls_move(const char *old_path, const char *new_path)
{
#if LS_WINDOWS
	BOOL b;
	WCHAR szOld[MAX_PATH], szNew[MAX_PATH];

	if (ls_utf8_to_wchar_buf(old_path, szOld, MAX_PATH) == -1)
		return -1;

	if (ls_utf8_to_wchar_buf(new_path, szNew, MAX_PATH) == -1)
		return -1;

	b = MoveFileW(szOld, szNew);
	if (!b)
		return ls_set_errno_win32(GetLastError());
	return 0;
#else
	int rc;
	
	rc = rename(old_path, new_path);
	if (rc == -1)
		return ls_set_errno(ls_errno_to_error(errno));
	return 0;
#endif // LS_WINDOWS
}

int ls_copy(const char *old_path, const char *new_path)
{
#if LS_WINDOWS
	BOOL b;
	WCHAR szOld[MAX_PATH], szNew[MAX_PATH];

	if (ls_utf8_to_wchar_buf(old_path, szOld, MAX_PATH) == -1)
		return -1;

	if (ls_utf8_to_wchar_buf(new_path, szNew, MAX_PATH) == -1)
		return -1;

	b = CopyFileW(szOld, szNew, FALSE);
	if (!b)
		return ls_set_errno_win32(GetLastError());
	return 0;
#elif LS_DARWIN
	int rc;
	copyfile_state_t s;

	if (!old_path || !new_path)
		return -1;
	
	s = copyfile_state_alloc();
	if (!s)
		return -1;
	
	rc = copyfile(old_path, new_path, s, COPYFILE_STAT | COPYFILE_DATA);
	
	copyfile_state_free(s);
	
	return rc == 0 ? 0 : -1;
#else
	int src_fd, dst_fd;
	ssize_t r;

	if (!old_path || !new_path)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	src_fd = open(old_path, O_RDONLY);
	if (src_fd == -1)
		return ls_set_errno(ls_errno_to_error(errno));

	dst_fd = open(new_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (dst_fd == -1)
	{
		ls_set_errno(ls_errno_to_error(errno));
		(void)close(src_fd);
		return r;
	}

	r = sendfile(dst_fd, src_fd, NULL, 0);
	if (r == -1)
		ls_set_errno(ls_errno_to_error(errno));

	(void)close(dst_fd);
	(void)close(src_fd);

	return r;
#endif // LS_WINDOWS
}

int ls_delete(const char *path)
{
#if LS_WINDOWS
	BOOL b;
	WCHAR szPath[MAX_PATH];

	if (ls_utf8_to_wchar_buf(path, szPath, MAX_PATH) == -1)
		return -1;

	b = DeleteFileW(szPath);
	if (!b)
		return ls_set_errno_win32(GetLastError());
	return 0;
#else
	int rc;

	if (!path)
		return ls_set_errno(LS_INVALID_ARGUMENT);
	
	rc = unlink(path);
	if (rc == -1)
		return ls_set_errno(ls_errno_to_error(errno));
	return 0;
#endif // LS_WINDOWS
}

int ls_createfile(const char *path, size_t size)
{
#if LS_WINDOWS
	HANDLE hFile;
	DWORD r;
	LARGE_INTEGER liSize = { .QuadPart = size };
	WCHAR szPath[MAX_PATH];

	if (ls_utf8_to_wchar_buf(path, szPath, MAX_PATH) == -1)
		return -1;

	hFile = CreateFileW(szPath, GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile == INVALID_HANDLE_VALUE)
		return ls_set_errno_win32(GetLastError());

	if (size > 0)
	{
		r = SetFilePointer(hFile, liSize.LowPart, &liSize.HighPart, FILE_BEGIN);
		if (r == INVALID_SET_FILE_POINTER)
		{
			ls_set_errno_win32(GetLastError());
			CloseHandle(hFile);
			return -1;
		}

		if (!SetEndOfFile(hFile))
		{
			ls_set_errno_win32(GetLastError());
			CloseHandle(hFile);
			return -1;
		}
	}

	CloseHandle(hFile);
	return 0;
#else
	int fd;
	int r;

	if (!path)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd == -1)
		return ls_set_errno(ls_errno_to_error(errno));

	if (size > 0)
	{
		r = ftruncate(fd, size);
		if (r == -1)
		{
			ls_set_errno(ls_errno_to_error(errno));
			(void)close(fd);
			return -1;
		}
	}

	(void)close(fd);
	return 0;
#endif // LS_WINDOWS
}

int ls_createdir(const char *path)
{
#if LS_WINDOWS
	WCHAR szPath[MAX_PATH];
	BOOL b;

	if (ls_utf8_to_wchar_buf(path, szPath, MAX_PATH) == -1)
		return -1;

	b = CreateDirectoryW(szPath, NULL);
	if (!b)
		return ls_set_errno_win32(GetLastError());
	return 0;
#else
	int rc;

	if (!path)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	rc = mkdir(path, 0777);
	if (rc == -1)
		return ls_set_errno(ls_errno_to_error(errno));
	return 0;
#endif // LS_WINDOWS
}

int ls_createdirs(const char *path)
{
	char *tmp, *cur;
	struct ls_stat st;
	int rc = -1;

	if (!path)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	tmp = ls_strdup(path);
	if (!tmp)
		return -1;

	cur = tmp;
	while ((cur = ls_strdir(cur)))
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
			rc = ls_set_errno(LS_ALREADY_EXISTS);
			break;
		}
	}

	ls_free(tmp);
	return rc;
}

ls_handle ls_pipe_create(const char *name, int flags)
{
#if LS_WINDOWS
	PHANDLE ph;
	HANDLE hPipe;
	WCHAR szName[MAX_PATH];
	WCHAR szPath[MAX_PATH];
	size_t cb;
	DWORD dwErr;
	DWORD dwOpenMode;

	cb = ls_utf8_to_wchar_buf(name, szName, MAX_PATH);
	if (cb == -1)
		return NULL;

	cb = ls_scbwprintf(szPath, sizeof(szPath), L"\\\\.\\pipe\\%s", szName);
	if (cb == -1)
		return NULL;

	ph = ls_handle_create(&FileClass);
	if (!ph)
		return NULL;

	dwOpenMode = PIPE_ACCESS_DUPLEX;
	if (flags & LS_FLAG_ASYNC)
		dwOpenMode |= FILE_FLAG_OVERLAPPED;

	hPipe = CreateNamedPipeW(szPath, PIPE_ACCESS_DUPLEX, PIPE_MODE, 1,
		PIPE_BUF_SIZE, PIPE_BUF_SIZE, 0, NULL);
	if (hPipe == INVALID_HANDLE_VALUE)
	{
		dwErr = GetLastError();
		ls_handle_dealloc(ph);
		ls_set_errno_win32(dwErr);
		return NULL;
	}

	*ph = hPipe;

	return ph;
#else
	ls_set_errno(LS_NOT_IMPLEMENTED);
	return NULL;
#endif // LS_WINDOWS
}

ls_handle ls_pipe_open(const char *name, int access)
{
#if LS_WINDOWS
	char szPath[MAX_PATH];
	size_t cb;

	if (!name)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	cb = ls_scbprintf(szPath, sizeof(szPath), "\\\\.\\pipe\\%s", name);
	if (cb == -1)
		return NULL;

	return ls_open(szPath, LS_FILE_READ | LS_FILE_WRITE | access,
		LS_SHARE_READ | LS_SHARE_WRITE, LS_OPEN_EXISTING);
#else
	ls_set_errno(LS_NOT_IMPLEMENTED);
	return NULL;
#endif // LS_WINDOWS
}
