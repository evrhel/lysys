#include <lysys/ls_file.h>

#include <lysys/ls_core.h>
#include <lysys/ls_memory.h>
#include <lysys/ls_shell.h>
#include <lysys/ls_stat.h>
#include <lysys/ls_string.h>
#include <lysys/ls_random.h>
#include <lysys/ls_thread.h>
#include <lysys/ls_event.h>
#include <lysys/ls_net.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ls_handle.h"
#include "ls_native.h"
#include "ls_sync_util.h"
#include "ls_util.h"
#include "ls_file_priv.h"
#include "ls_event_priv.h"

#if LS_WINDOWS
#define PIPE_BUF_SIZE 4096
#define PIPE_MODE (PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT)
#define PIPE_PREFIX L"\\\\.\\pipe\\"

#else
#define PIPE_PREFIX "/tmp/"
#endif // LS_WINDOWS

// -1 for null terminator
#define MAX_PIPE_PATH (sizeof(PIPE_PREFIX) + LS_MAX_PIPE_NAME - 1)

static void ls_file_dtor(ls_file_t *pf)
{
#if LS_WINDOWS
	CloseHandle(pf->hFile);
#else
	(void)close(pf->fd);
#endif // LS_WINDOWS
}

static const struct ls_class FileClass = {
	.type = LS_FILE,
	.cb = sizeof(ls_file_t),
	.dtor = (ls_dtor_t)&ls_file_dtor,
	.wait = NULL
};

ls_handle ls_open(const char *path, int access, int share, int create)
{
#if LS_WINDOWS
	ls_file_t *pf;
	HANDLE hFile;
	DWORD dwDesiredAccess;
	DWORD dwFlagsAndAttributes;
	WCHAR szPath[MAX_PATH];

	if (ls_utf8_to_wchar_buf(path, szPath, MAX_PATH) == -1)
		return NULL;	

	dwDesiredAccess = ls_get_access_rights(access);
	dwFlagsAndAttributes = ls_get_flags_and_attributes(access);

	pf = ls_handle_create(&FileClass, access);
	if (!pf)
		return NULL;

	hFile = CreateFileW(szPath, dwDesiredAccess, share, NULL, create,
		dwFlagsAndAttributes, NULL);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		ls_set_errno_win32(GetLastError());
		ls_handle_dealloc(pf);
		return NULL;
	}

	pf->hFile = hFile;
	return pf;
#else
	int *pfd;
	int fd;
	int oflags;

	if (!path)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	pfd = ls_handle_create(&FileClass, 0);
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
	ls_file_t *pf;
	BOOL bRet;
	LARGE_INTEGER liDist = { .QuadPart = offset };
	LARGE_INTEGER liNewPointer;
	int flags;

	if (LS_HANDLE_IS_TYPE(file, LS_SOCKET))
		return ls_set_errno(LS_INVALID_HANDLE);

	pf = ls_resolve_file(file, &flags);
	if (!pf)
		return -1;

	if (flags & LS_FLAG_ASYNC)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	bRet = SetFilePointerEx(pf->hFile, liDist, &liNewPointer, origin);
	if (!bRet)
		return ls_set_errno_win32(GetLastError());
	return liNewPointer.QuadPart;
#else
	struct ls_file *pf;
	off_t r;
	int flags;

	if (LS_HANDLE_IS_TYPE(file, LS_SOCKET))
		return ls_set_errno(LS_INVALID_HANDLE);

	pf = ls_resolve_file(file, &flags);
	if (!pf)
		return -1;

	if (pf->fd == -1)
		return 0;

	r = lseek(pf->fd, offset, origin);
	if (r == -1)
		return ls_set_errno(ls_errno_to_error((int)r));
	return r;
#endif // LS_WINDOWS
}

size_t ls_read(ls_handle fh, void *buffer, size_t size)
{
#if LS_WINDOWS
	struct ls_file *pf;
	BOOL bRet;
	DWORD dwRead, dwToRead;
	size_t remaining;
	int flags;

	if (LS_HANDLE_IS_TYPE(fh, LS_SOCKET))
		return ls_net_recv(fh, buffer, size);

	pf = ls_resolve_file(fh, &flags);
	if (!pf)
		return -1;

	if (flags & LS_FLAG_ASYNC)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	if (!(flags & LS_FILE_READ))
		return ls_set_errno(LS_INVALID_ARGUMENT);

	dwToRead = (DWORD)(size & 0xffffffff);

	remaining = size;

	while (remaining != 0)
	{
		bRet = ReadFile(pf->hFile, buffer, dwToRead, &dwRead, NULL);
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
	struct ls_file *pf;
	size_t bytes_read;
	size_t remaining;
	int flags;

	if (LS_HANDLE_IS_TYPE(fh, LS_SOCKET))
		return ls_net_recv(fh, buffer, size);

	pf = ls_resolve_file(fh, &flags);
	if (!pf)
		return -1;
	
	if (pf->fd == -1)
		return 0;

	if (flags & LS_FLAG_ASYNC)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	if (!(flags & LS_FILE_READ))
		return ls_set_errno(LS_INVALID_ARGUMENT);

	remaining = size;
	while (remaining != 0)
	{
		bytes_read = (size_t)read(pf->fd, buffer, remaining);
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
	struct ls_file *pf;
	BOOL bRet;
	DWORD dwWritten, dwToWrite;
	size_t remaining;
	int flags;

	if (LS_HANDLE_IS_TYPE(fh, LS_SOCKET))
		return ls_net_send(fh, buffer, size);

	pf = ls_resolve_file(fh, &flags);
	if (!pf)
		return -1;

	if (flags & LS_FLAG_ASYNC)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	if (!(flags & LS_FILE_WRITE))
		return ls_set_errno(LS_INVALID_ARGUMENT);

	dwToWrite = (DWORD)(size & 0xffffffff);

	remaining = size;

	while (remaining != 0)
	{
		bRet = WriteFile(pf->hFile, buffer, dwToWrite, &dwWritten, NULL);
		if (!bRet)
			return ls_set_errno_win32(GetLastError());

		remaining -= dwWritten;
		dwToWrite = (DWORD)(remaining & 0xffffffff);
		buffer = (const uint8_t *)buffer + dwWritten; // advance buffer

	}

	return size - remaining;
#else
	struct ls_file *pf;
	size_t bytes_written;
	size_t remaining;
	int flags;

	if (LS_HANDLE_IS_TYPE(fh, LS_SOCKET))
		return ls_net_send(fh, buffer, size);

	pf = ls_resolve_file(fh, &flags);
	if (!pf)
		return -1;

	if (pf->fd == -1)
		return size;

	if (flags & LS_FLAG_ASYNC)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	if (!(flags & LS_FILE_WRITE))
		return ls_set_errno(LS_INVALID_ARGUMENT);

	remaining = size;
	while (remaining != 0)
	{
		bytes_written = (size_t)write(pf->fd, buffer, remaining);
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
	int flags;

	if (LS_HANDLE_IS_TYPE(file, LS_SOCKET))
		return 0;

	hFile = ls_resolve_file(file, &flags);
	if (!hFile)
		return -1;

	if (!(flags & LS_FILE_WRITE))
		return ls_set_errno(LS_INVALID_ARGUMENT);

	b = FlushFileBuffers(hFile);
	if (!b)
		return ls_set_errno_win32(GetLastError());
	return 0;
#else
	struct ls_file *pf;
	int rc;
	int flags;

	if (LS_HANDLE_IS_TYPE(file, LS_SOCKET))
		return 0;

	pf = ls_resolve_file(file, &flags);
	if (!pf)
		return -1;

	if (pf->fd == -1)
		return 0;

	if (!(flags & LS_FILE_WRITE))
		return ls_set_errno(LS_INVALID_ARGUMENT);

	rc = fsync(pf->fd);
	if (rc == -1)
		return ls_set_errno(ls_errno_to_error(errno));
	return 0;
#endif // LS_WINDOWS
}

struct ls_aio
{
	ls_lock_t lock;
#if LS_WINDOWS
	OVERLAPPED ov;
	HANDLE hFile;
#else
	ls_cond_t cond;
	struct aiocb aiocb;
	size_t bytes_transferred;
	int status;
	int error;
#endif // LS_WINDOWS
};

static void ls_aio_dtor(struct ls_aio *aio)
{
#if LS_WINDOWS
#else
	cond_destroy(&aio->cond);
	lock_destroy(&aio->lock);
#endif // LS_WINDOWS
}

static int ls_aio_wait(struct ls_aio *aio, unsigned long ms)
{
#if LS_WINDOWS
	DWORD dwRet;

	dwRet = WaitForSingleObjectEx(aio->ov.hEvent, ms, TRUE);

	if (dwRet == WAIT_OBJECT_0)
		return 0;

	if (dwRet == WAIT_TIMEOUT)
		return 1;

	return ls_set_errno_win32(GetLastError());
#else
	int rc;

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

#if LS_POSIX

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

static void ls_aio_handler(union sigval sv)
{
	struct ls_aio *aio = sv.sival_ptr;

	lock_lock(&aio->lock);

	(void)ls_aio_check_error(aio);

	lock_unlock(&aio->lock);
}

#endif // LS_POSIX

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
	struct ls_file *pf;
	int rc;
	int flags;

	pf = ls_resolve_file(fh, &flags);
	if (!pf)
		return NULL;

	if (!(flags & LS_FLAG_ASYNC))
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	aio = ls_handle_create(&AioClass, flags);
	if (!aio)
		return NULL;

	rc = lock_init(&aio->lock);
	if (rc == -1)
	{
		ls_handle_dealloc(aio);
		return NULL;
	}

	aio->ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
	if (!aio->ov.hEvent)
	{
		ls_set_errno_win32(GetLastError());
		lock_destroy(&aio->lock);
		ls_handle_dealloc(aio);
		return NULL;
	}

	aio->hFile = pf->hFile;

	return aio;
#else
	struct ls_aio *aio;
	ls_file_t *pf;
	int rc;
	struct sigevent *sev;
	int flags;

	if (LS_HANDLE_IS_TYPE(fh, LS_SOCKET))
		return ls_set_errno(LS_INVALID_HANDLE);

	pf = ls_resolve_file(fh, &flags);
	if (!pf)
		return NULL;

	aio = ls_handle_create(&AioClass, flags);
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

	aio->aiocb.aio_fildes = pf->fd;

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

#if LS_WINDOWS


#endif // LS_WINDOWS

int ls_aio_read(ls_handle aioh, uint64_t offset, volatile void *buffer, size_t size)
{
#if LS_WINDOWS
	struct ls_aio *aio;
	BOOL b;
	DWORD dwToRead;
	DWORD dwErr;
	ULARGE_INTEGER uliOffset = { .QuadPart = offset };
	DWORD dwTransferred;

	if (!aioh)
		return ls_set_errno(LS_INVALID_HANDLE);

	if (!buffer || size == 0)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	if (!(LS_HANDLE_INFO(aioh)->flags & LS_FILE_READ))
		return ls_set_errno(LS_INVALID_ARGUMENT);

	aio = aioh;
	lock_lock(&aio->lock);

	// check if the previous operation has completed
	b = GetOverlappedResult(aio->hFile, &aio->ov, &dwTransferred, FALSE);
	if (!b)
	{
		dwErr = GetLastError();
		lock_unlock(&aio->lock);
		return ls_set_errno_win32(dwErr);
	}

	aio->ov.Offset = uliOffset.LowPart;
	aio->ov.OffsetHigh = uliOffset.HighPart;

	if (!ResetEvent(aio->ov.hEvent))
	{
		dwErr = GetLastError();
		lock_unlock(&aio->lock);
		return ls_set_errno_win32(dwErr);
	}

	dwToRead = (DWORD)(size & 0xffffffff);
	b = ReadFile(aio->hFile, (LPVOID)buffer, dwToRead, NULL, &aio->ov);
	if (!b)
	{
		dwErr = GetLastError();
		if (dwErr != ERROR_IO_PENDING)
		{
			ls_set_errno_win32(dwErr);
			SetEvent(aio->ov.hEvent); // prevent deadlock

			lock_unlock(&aio->lock);
			return -1;
		}
	}

	lock_unlock(&aio->lock);
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

	if (aio->aiocb.aio_fildes == -1)
	{
		// devnull, no need to perform any I/O
		aio->status = LS_AIO_COMPLETED;
		aio->bytes_transferred = 0;
		cond_broadcast(&aio->cond);
		lock_unlock(&aio->lock);
		return 0;
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
	DWORD dwTransferred;

	if (!aioh)
		return ls_set_errno(LS_INVALID_HANDLE);

	if (!(LS_HANDLE_INFO(aioh)->flags & LS_FILE_WRITE))
		return ls_set_errno(LS_INVALID_ARGUMENT);

	aio = aioh;
	lock_lock(&aio->lock);

	// check if the previous operation has completed
	b = GetOverlappedResult(aio->hFile, &aio->ov, &dwTransferred, FALSE);
	if (!b)
	{
		dwErr = GetLastError();
		lock_unlock(&aio->lock);
		return ls_set_errno_win32(dwErr);
	}

	aio->ov.Offset = uliOffset.LowPart;
	aio->ov.OffsetHigh = uliOffset.HighPart;

	if (!ResetEvent(aio->ov.hEvent))
	{
		dwErr = GetLastError();
		lock_unlock(&aio->lock);
		return ls_set_errno_win32(dwErr);
	}

	dwToWrite = (DWORD)(size & 0xffffffff);
	b = WriteFile(aio->hFile, (LPCVOID)buffer, dwToWrite, NULL, &aio->ov);
	if (!b)
	{
		dwErr = GetLastError();
		if (dwErr != ERROR_IO_PENDING)
		{
			ls_set_errno_win32(dwErr);
			SetEvent(aio->ov.hEvent); // prevent deadlock

			lock_unlock(&aio->lock);
			return -1;
		}
	}

	lock_unlock(&aio->lock);
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

	if (aio->aiocb.aio_fildes == -1)
	{
		// devnull, no need to perform any I/O
		aio->status = LS_AIO_COMPLETED;
		aio->bytes_transferred = size;
		cond_broadcast(&aio->cond);
		lock_unlock(&aio->lock);
		return 0;
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

	if (!aioh)
		return ls_set_errno(LS_INVALID_HANDLE);

	lock_lock(&aio->lock);

	b = GetOverlappedResult(aio->hFile, &aio->ov, &dwTransferred, FALSE);
	if (!b)
	{
		dwErr = GetLastError();

		lock_unlock(&aio->lock);

		if (dwErr == ERROR_IO_INCOMPLETE)
			return LS_AIO_PENDING;

		if (dwErr == ERROR_OPERATION_ABORTED)
			return LS_AIO_CANCELED;

		return ls_set_errno_win32(dwErr);
	}

	lock_unlock(&aio->lock);

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
			*transferred = 0;
		}
		else
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

	b = CancelIoEx(aio->hFile, &aio->ov);
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
	char *tmp;
	char *next;
	struct ls_stat st;
	int rc;

	tmp = ls_strdup(path);
	if (!tmp)
		return -1;

	next = tmp;
	while (next)
	{
		next = strchr(next, LS_PATH_SEP);

		if (next)
			*next = 0;

		rc = ls_stat(tmp, &st);

		if (rc == -1)
		{
			rc = ls_createdir(tmp);
			if (rc == -1)
			{
				ls_free(tmp);
				return -1;
			}
		}
		else if (st.type != LS_FT_DIR)
		{
			ls_free(tmp);
			return ls_set_errno(LS_ALREADY_EXISTS);
		}

		if (next)
			*next = LS_PATH_SEP, next++;
	}

	ls_free(tmp);
	return 0;
}

static void ls_pipe_dtor(ls_pipe_t *pp)
{
#if LS_WINDOWS
	if (pp->ov.hEvent)
		CloseHandle(pp->ov.hEvent);
	CloseHandle(pp->hPipe);
#else
	(void)close(pp->fd);
	ls_free(pp->path);
#endif // LS_WINDOWS
}

static const struct ls_class PipeClass = {
	.type = LS_PIPE,
	.cb = sizeof(ls_pipe_t),
	.dtor = (ls_dtor_t)&ls_pipe_dtor,
	.wait = NULL
};

int ls_pipe(ls_handle *read, ls_handle *write, int flags)
{
#if LS_WINDOWS
	struct ls_file *pread, *pwrite;
	HANDLE hRead, hWrite;
	DWORD dwErr;
	WCHAR szName[MAX_PATH];
	size_t len;
	DWORD dwWriteMode, dwReadMode;
	uint64_t serial;
	int read_async, write_async;
	int handle_flags;

	if (!read || !write)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	read_async = !!(flags & LS_ANON_PIPE_READ_ASYNC);
	write_async = !!(flags & LS_ANON_PIPE_WRITE_ASYNC);

	serial = ls_rand_uint64();

	len = ls_scbwprintf(
		szName,
		sizeof(szName),
		PIPE_PREFIX L"lysys.%lu.%lu.%llu",
		GetCurrentProcessId(),
		GetCurrentThreadId(),
		serial);
	if (len == -1)
		return -1;

	handle_flags = LS_FILE_READ | (read_async ? LS_FLAG_ASYNC : 0);
	pread = ls_handle_create(&FileClass, handle_flags);
	if (!pread)
		return -1;

	handle_flags = LS_FILE_WRITE | (write_async ? LS_FLAG_ASYNC : 0);
	pwrite = ls_handle_create(&FileClass, handle_flags);
	if (!pwrite)
	{
		ls_handle_dealloc(pread);
		return -1;
	}

	dwWriteMode = write_async ? FILE_FLAG_OVERLAPPED : 0;
	hRead = CreateNamedPipeW(
		szName,
		PIPE_ACCESS_INBOUND | dwWriteMode,
		PIPE_TYPE_BYTE | PIPE_WAIT,
		1,
		PIPE_BUF_SIZE,
		PIPE_BUF_SIZE,
		0,
		NULL);

	if (hRead == INVALID_HANDLE_VALUE)
	{
		dwErr = GetLastError();
		ls_handle_dealloc(pwrite);
		ls_handle_dealloc(pread);
		return ls_set_errno_win32(dwErr);
	}

	dwReadMode = read_async ? FILE_FLAG_OVERLAPPED : 0;
	hWrite = CreateFileW(
		szName,
		GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | dwReadMode,
		NULL);

	if (hWrite == INVALID_HANDLE_VALUE)
	{
		dwErr = GetLastError();
		CloseHandle(hRead);
		ls_handle_dealloc(pwrite);
		ls_handle_dealloc(pread);
		return ls_set_errno_win32(dwErr);
	}

	pread->hFile = hRead;
	pwrite->hFile = hWrite;

	*read = pread;
	*write = pwrite;

	return 0;
#else
	int rc;
	int fds[2];
	int *pread, *pwrite;

	if (!read || !write)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	pread = ls_handle_create(&FileClass, 0);
	if (!pread)
		return -1;

	pwrite = ls_handle_create(&FileClass, 0);
	if (!pwrite)
	{
		rc = _ls_errno;
		ls_handle_dealloc(pread);
		return ls_set_errno(rc);
	}

	rc = pipe(fds);
	if (rc == -1)
	{
		rc = ls_errno_to_error(errno);
		ls_handle_dealloc(pwrite);
		ls_handle_dealloc(pread);
		return ls_set_errno(rc);
	}

	*pread = fds[0];
	*pwrite = fds[1];

	*read = pread;
	*write = pwrite;

	return 0;
#endif // LS_WINDOWS
}

ls_handle ls_named_pipe(const char *name, int flags, int wait)
{
#if LS_WINDOWS
	ls_pipe_t *pp;
	HANDLE hPipe;
	WCHAR szName[MAX_PATH];
	size_t len;
	DWORD dwErr;
	DWORD dwMode;
	int is_async;
	BOOL b;
	int handle_flags;

	is_async = !!(flags & LS_FLAG_ASYNC);

	len = ls_scbwprintf(szName, sizeof(szName), PIPE_PREFIX L"%s", name);
	if (len == -1)
		return NULL;

	handle_flags = LS_FILE_READ | LS_FILE_WRITE;
	if (is_async)
		handle_flags |= LS_FLAG_ASYNC;

	pp = ls_handle_create(&PipeClass, handle_flags);
	if (!pp)
		return NULL;

	dwMode = is_async ? FILE_FLAG_OVERLAPPED : 0;

	hPipe = CreateNamedPipeW(szName,
		PIPE_ACCESS_DUPLEX | dwMode,
		PIPE_MODE, 1, PIPE_BUF_SIZE, PIPE_BUF_SIZE, 0, NULL);
	if (hPipe == INVALID_HANDLE_VALUE)
	{
		dwErr = GetLastError();
		ls_handle_dealloc(pp);
		ls_set_errno_win32(dwErr);
		return NULL;
	}

	pp->hPipe = hPipe;

	if (is_async)
	{
		b = ConnectNamedPipe(hPipe, &pp->ov);
		if (!b)
		{
			dwErr = GetLastError();
			if (dwErr != ERROR_IO_PENDING)
			{
				CloseHandle(hPipe);
				ls_handle_dealloc(pp);
				ls_set_errno_win32(dwErr);
				return NULL;
			}
		}
	}
	else if (!wait)
	{
		b = ConnectNamedPipe(hPipe, NULL);
		if (!b)
		{
			dwErr = GetLastError();
			CloseHandle(hPipe);
			ls_handle_dealloc(pp);
			ls_set_errno_win32(dwErr);
			return NULL;
		}
	}

	return pp;
#else
	int rc;
	int fd;
	struct ls_pipe *pp;
	char path[MAX_PIPE_PATH];
	size_t cb;
	int is_async;
	int handle_flags;

	if (!name)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	cb = ls_scbprintf(path, sizeof(path), PIPE_PREFIX "%s", name);
	if (cb == -1)
		return NULL;

	is_async = !!(flags & LS_FLAG_ASYNC);
	handle_flags = LS_FILE_READ | LS_FILE_WRITE;
	if (is_async)
		handle_flags |= LS_FLAG_ASYNC;

	pp = ls_handle_create(&FileClass, handle_flags);
	if (!pp)
		return NULL;
	
	rc = mkfifo(path, 0666);
	if (rc == -1)
	{
		ls_set_errno(ls_errno_to_error(errno));
		ls_handle_dealloc(pp);
		return NULL;
	}

	if (is_async)
	{
		fd = open(path, O_RDWR | O_NONBLOCK);
		if (fd == -1)
		{
			ls_set_errno(ls_errno_to_error(errno));
			ls_handle_dealloc(pp);
			return NULL;
		}

		pp->connected = 1;
		pp->fd = fd;
	}
	else if (wait)
	{
		fd = open(path, O_RDWR);
		if (fd == -1)
		{
			ls_set_errno(ls_errno_to_error(errno));
			ls_handle_dealloc(pp);
			return NULL;
		}

		pp->connected = 1;
		pp->fd = fd;
	}
	else
	{
		pp->path = ls_strdup(path);
		if (!pp->path)
		{
			ls_handle_dealloc(pp);
			return NULL;
		}
	}

	return pp;
#endif // LS_WINDOWS
}

int ls_named_pipe_wait(ls_handle fh, unsigned long timeout)
{
#if LS_WINDOWS
	ls_pipe_t *pp;
	BOOL b;
	DWORD dwRet;
	int flags;

	pp = ls_resolve_pipe(fh, &flags);
	if (!pp)
		return -1;

	if (pp->connected)
		return 0;

	if (timeout == 0)
		return 1;

	if (flags & LS_FLAG_ASYNC)
	{
		dwRet = WaitForSingleObject(pp->ov.hEvent, timeout);
		if (dwRet == WAIT_OBJECT_0)
		{
			// can close the event handle
			CloseHandle(pp->ov.hEvent);
			pp->ov.hEvent = NULL;

			pp->connected = 1;
			return 0;
		}

		if (dwRet == WAIT_TIMEOUT)
			return 1;

		return ls_set_errno_win32(GetLastError());
	}

	b = ConnectNamedPipe(pp->hPipe, NULL);
	if (!b)
		return ls_set_errno_win32(GetLastError());

	pp->connected = 1;
	return 0;
#else
	ls_pipe_t *pp;
	int flags;

	pp = ls_resolve_pipe(fh, &flags);
	if (!pp)
		return -1;

	if (pp->connected)
		return 0;

	if (timeout == 0)
		return 1;

	pp->fd = open(pp->path, O_RDWR);
	if (pp->fd == -1)
		return ls_set_errno(ls_errno_to_error(errno));

	pp->connected = 1;
	return 0;
#endif // LS_WINDOWS
}

ls_handle ls_pipe_open(const char *name, int access, unsigned long timeout)
{
#if LS_WINDOWS
	WCHAR szPath[MAX_PATH];
	size_t cb;
	BOOL b;
	ls_pipe_t *pp;
	int is_async;
	DWORD dwAccess, dwFlagsAndAttributes;

	if (!name)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	cb = ls_scbwprintf(szPath, sizeof(szPath), PIPE_PREFIX L"%s", name);
	if (cb == -1)
		return NULL;

	// wait for the pipe to become available
	if (timeout > 0)
	{
		b = WaitNamedPipeW(szPath, timeout);
		if (!b)
		{
			ls_set_errno_win32(GetLastError());
			return NULL;
		}
	}

	pp = ls_handle_create(&PipeClass, access);
	if (!pp)
		return NULL;

	is_async = !!(access & LS_FLAG_ASYNC);

	dwAccess = 0;

	if (access & LS_FILE_READ)
		dwAccess |= GENERIC_READ;

	if (access & LS_FILE_WRITE)
		dwAccess |= GENERIC_WRITE;

	dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
	if (is_async)
		dwFlagsAndAttributes |= FILE_FLAG_OVERLAPPED;

	// open the pipe
	pp->hPipe = CreateFileW(
		szPath,
		dwAccess,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		dwFlagsAndAttributes,
		NULL);
	if (pp->hPipe == INVALID_HANDLE_VALUE)
	{
		ls_set_errno_win32(GetLastError());
		ls_handle_dealloc(pp);
		return NULL;
	}

	return pp;
#else
	char path[MAX_PIPE_PATH];
	size_t cb;
	int *pfd;
	int fd;
	int is_async;
	int flags;

	if (!name)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	cb = ls_scbprintf(path, sizeof(path), PIPE_PREFIX "%s", name);
	if (cb == -1)
		return NULL;

	is_async = !!(access & LS_FLAG_ASYNC);

	pfd = ls_handle_create(&FileClass, access);
	if (!pfd)
		return NULL;

	flags = 0;
	if (access & LS_FILE_READ)
		flags |= O_RDONLY;

	if (access & LS_FILE_WRITE)
		flags |= O_WRONLY;

	if (is_async)
		flags |= O_NONBLOCK;

	fd = open(path, flags);
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
