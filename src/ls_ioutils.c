#include <lysys/ls_ioutils.h>

#include <stdlib.h>
#include <memory.h>
#include <stdarg.h>
#include <stdio.h>

#include <lysys/ls_file.h>
#include <lysys/ls_core.h>
#include <lysys/ls_thread.h>
#include <lysys/ls_event.h>

#include "ls_native.h"
#include "ls_file_priv.h"
#include "ls_buffer.h"
#include "ls_handle.h"
#include "ls_sync_util.h"

#define BUFFER_SIZE 1024

void *ls_read_all_bytes(ls_handle fh, size_t *size)
{
	uint8_t *result, *tmp;
	size_t total_size;

	uint8_t buf[BUFFER_SIZE];
	size_t bytes_read;

	if (!fh || !size)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	result = NULL;
	total_size = 0;
	
	for (;;)
	{
		bytes_read = ls_read(fh, buf, BUFFER_SIZE);
		if (bytes_read == -1)
		{
			ls_free(result);
			return NULL;
		}

		if (bytes_read == 0)
			break;

		tmp = ls_realloc(result, total_size + bytes_read);
		if (!tmp)
		{
			ls_free(result);
			return NULL;
		}

		result = tmp;
		memcpy(result + total_size, buf, bytes_read);
		total_size += bytes_read;
	}

	*size = total_size;
	return result;
}

char *ls_readline(ls_handle fh, size_t *len)
{
	char *result, *tmp;
	size_t total_size;
	size_t capacity;

	char c;
	size_t bytes_read;

	if (!fh)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	capacity = BUFFER_SIZE;
	total_size = 0;
	result = ls_malloc(capacity);
	if (!result)
		return NULL;

	for (;;)
	{
		bytes_read = ls_read(fh, &c, 1); // only read one byte at a time
		if (bytes_read == -1)
		{
			ls_free(result);
			return NULL;
		}

		if (bytes_read == 0)
			break;

		// ignore '\0' and '\r' characters
		if (c == '\0' || c == '\r')
			continue;

		if (c == '\n')
			break;

		// grow buffer if needed
		if (total_size == capacity)
		{
			capacity += BUFFER_SIZE;
			tmp = ls_realloc(result, capacity);
			if (!tmp)
			{
				ls_free(result);
				return NULL;
			}

			result = tmp;
		}

		result[total_size++] = c;
	}

	tmp = ls_realloc(result, total_size + 1);
	if (!tmp)
	{
		ls_free(result);
		return NULL;
	}

	result = tmp;
	result[total_size] = 0;

	if (len)
		*len = total_size;

	return result;
}

void *ls_read_file(const char *filename, size_t *size)
{
	ls_handle fh;
	void *result;

	if (!filename || !size)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	fh = ls_open(filename, LS_FILE_READ, LS_SHARE_READ, LS_OPEN_EXISTING);
	if (!fh)
		return NULL;

	result = ls_read_all_bytes(fh, size);

	ls_close(fh);

	return result;
}

size_t ls_write_file(const char *filename, const void *data, size_t size)
{
	ls_handle fh;
	size_t bytes_written;

	if (!filename || !data)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return -1;
	}

	fh = ls_open(filename, LS_FILE_WRITE, 0, LS_CREATE_ALWAYS);
	if (!fh)
		return -1;

	bytes_written = ls_write(fh, data, size);
	
	ls_close(fh);

	return bytes_written;
}

size_t ls_fprintf(ls_handle fh, const char *format, ...)
{
	va_list args;
	size_t result;

	va_start(args, format);
	result = ls_vfprintf(fh, format, args);
	va_end(args);

	return result;
}

size_t ls_vfprintf(ls_handle fh, const char *format, va_list args)
{
	char *buf;
	int rc;
	size_t count;

	if (!fh || !format)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return -1;
	}

	rc = vsnprintf(NULL, 0, format, args);
	if (rc < 0)
	{
		ls_set_errno(LS_UNKNOWN_ERROR);
		return -1;
	}

	count = rc;
	buf = ls_malloc(count + 1);
	if (!buf)
		return -1;

	vsnprintf(buf, count + 1, format, args);

	count = ls_write(fh, buf, count);

	ls_free(buf);

	return count;
}
