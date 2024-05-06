#include <lysys/ls_core.h>

#include <stdlib.h>
#include <memory.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>

#include <lysys/ls_time.h>
#include <lysys/ls_sync.h>
#include <lysys/ls_thread.h>
#include <lysys/ls_sysinfo.h>

#include "ls_native.h"

int ls_errno(void) { return _ls_errno; }

void ls_perror(const char *msg)
{
	if (msg)
		fprintf(stderr, "%s: ", msg);
	fprintf(stderr, "%s\n", ls_strerror(_ls_errno));
}

const char *ls_strerror(int err)
{
	switch (err)
	{
	default:
	case LS_UNKNOWN_ERROR: return "Unknown error";
	case LS_SUCCESS: return "Success";
	case LS_INVALID_HANDLE: return "A handle is invalid";
	case LS_OUT_OF_MEMORY: return "There is insufficient memory to complete the operation";
	case LS_INVALID_ARGUMENT: return "One or more arguments are invalid";
	case LS_INVALID_STATE: return "An invalid state was detected";
	case LS_NOT_WAITABLE: return "The object is not waitable";
	case LS_ACCESS_DENIED: return "Access denied";
	case LS_FILE_NOT_FOUND: return "File not found";
	case LS_BUFFER_TOO_SMALL: return "Buffer too small, retry with a larger buffer";
	case LS_INVALID_ENCODING: return "Invalid character encoding";
	case LS_SHARING_VIOLATION: return "File sharing violation";
	case LS_OUT_OF_RANGE: return "A value is out of range";
	case LS_NOT_SUPPORTED: return "The operation is not supported";
	case LS_PATH_NOT_FOUND: return "The path was not found";
	case LS_END_OF_FILE: return "The end of the file has been reached";
	case LS_ALREADY_EXISTS: return "The object already exists";
	case LS_NOT_FOUND: return "The object was not found";
	case LS_BAD_PIPE: return "The pipe is broken";
	case LS_NO_MORE_FILES: return "No more files";
	case LS_NO_DATA: return "No data available";
	case LS_NOT_READY: return "The object is not ready";
	case LS_DEADLOCK: return "A deadlock was detected";
	case LS_INTERRUPTED: return "The operation was interrupted";
	case LS_IO_ERROR: return "An I/O error occurred";
	case LS_DISK_FULL: return "The disk is full";
	case LS_BUSY: return "The resource is busy";
	case LS_TIMEDOUT: return "The operation timed out";
	case LS_INVALID_PATH: return "The path is invalid";
	case LS_INVALID_IMAGE: return "The image is invalid";
	case LS_CANCELED: return "The operation was canceled";
	case LS_INTERNAL_ERROR: return "An internal error occurred";
	case LS_NOT_IMPLEMENTED: return "The operation is not implemented";
	}
}

size_t ls_substr(const char *s, size_t n, char *buf)
{
	size_t maxlen;
	
	if (!s || !buf)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	maxlen = strlen(s);
	if (n > maxlen)
		n = maxlen;

	memcpy(buf, s, n);
	buf[n] = 0;
	return n;
}

void *ls_malloc(size_t size)
{
	void *p;

	p = malloc(size);
	if (!p)
		ls_set_errno(LS_OUT_OF_MEMORY);
	return p;
}

void *ls_calloc(size_t count, size_t size)
{
	void *p;

	p = calloc(count, size);
	if (!p)
		ls_set_errno(LS_OUT_OF_MEMORY);
	return p;
}

void *ls_realloc(void *ptr, size_t size)
{
	void *p;

	p = realloc(ptr, size);
	if (!p)
		ls_set_errno(LS_OUT_OF_MEMORY);
	return p;
}

void ls_free(void *ptr) { free(ptr); }

char *ls_strdup(const char *s)
{
	char *r;
	size_t len;

	if (!s)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	len = strlen(s) + 1;

	r = ls_malloc(len);
	if (!r)
		return NULL;
	return memcpy(r, s, len);
}
