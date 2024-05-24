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

struct ls_io_buf
{
	ls_handle fh;
	int access;
	int should_run;

	ls_handle read_aio;
	struct ls_buffer read_buf;

	ls_handle write_aio;
	struct ls_buffer write_buf;
	int do_flush;

	ls_lock_t lock;
	ls_cond_t cond;

	ls_handle read_thread;
	ls_handle write_thread;
};

static void ls_io_buf_dtor(struct ls_io_buf *buf)
{
	lock_lock(&buf->lock);

	if (buf->read_aio)
		ls_aio_cancel(buf->read_aio);

	if (buf->write_aio)
		ls_aio_cancel(buf->write_aio);

	buf->should_run = 0;

	cond_broadcast(&buf->cond);

	lock_unlock(&buf->lock);

	if (buf->read_thread)
	{
		ls_wait(buf->read_thread);
		ls_close(buf->read_thread);
	}

	if (buf->write_thread)
	{
		ls_wait(buf->write_thread);
		ls_close(buf->write_thread);
	}

	lock_destroy(&buf->lock);
}

static int ls_io_buf_read_worker(struct ls_io_buf *buf)
{
	int rc;
	volatile uint8_t read_buf;
	size_t transferred;

	if (!buf->read_aio)
		return 0;

	lock_lock(&buf->lock);

	for (;;)
	{
		if (!buf->should_run)
			break;

		// read one byte at a time, since we don't know how much data is available
		rc = ls_aio_read(buf->read_aio, 0, &read_buf, 1, NULL, NULL);
		if (rc == -1)
		{
			buf->should_run = 0;
			break;
		}

		lock_unlock(&buf->lock);

	retry:
		// wait for read to complete
		rc = ls_wait(buf->read_aio);
		if (rc == -1)
		{
			lock_lock(&buf->lock);
			buf->should_run = 0;
			break;
		}

		// check the status of the previous read
		rc = ls_aio_status(buf->read_aio, &transferred, 0);
		switch (rc)
		{
		case LS_AIO_COMPLETED:
			lock_lock(&buf->lock);

			// write the read data to the client buffer
			rc = ls_buffer_put_char(&buf->read_buf, read_buf);
			if (rc == -1)
				buf->should_run = 0;

			break;
		default:
		case LS_AIO_CANCELED:
		case LS_AIO_ERROR:
			lock_lock(&buf->lock);
			buf->should_run = 0;
			break;
		case LS_AIO_PENDING:
			goto retry;
		}
	}

	ls_close(buf->read_aio), buf->read_aio = NULL;

	lock_unlock(&buf->lock);
	return 0;
}

static int ls_io_buf_write_worker(struct ls_io_buf *buf)
{
	int rc;
	volatile uint8_t write_buf[BUFFER_SIZE];
	size_t write_size = 0;
	size_t transferred;
	size_t pending;
	size_t empty_space;
	size_t remain;

	if (!buf->write_aio)
		return 0;

	lock_lock(&buf->lock);

	for (;;)
	{
		// wait for data to write
		while (buf->should_run && !buf->do_flush && ls_buffer_size(&buf->write_buf) == 0)
			cond_wait(&buf->cond, &buf->lock, LS_INFINITE);

		if (!buf->should_run)
			break;

		if (buf->do_flush)
		{
			ls_flush(buf->fh);
			buf->do_flush = 0;
		}

		lock_unlock(&buf->lock);

	retry:
		rc = ls_wait(buf->write_aio);
		if (rc == -1)
		{
			lock_lock(&buf->lock);
			buf->should_run = 0;
			break;
		}

		// check the status of the previous write
		rc = ls_aio_status(buf->write_aio, &transferred, 0);
		switch (rc)
		{
		case LS_AIO_COMPLETED:
			lock_lock(&buf->lock);

			if (transferred < write_size)
			{
				// not a full write, shift remaining data to the beginning of the buffer
				memmove((uint8_t *)write_buf, (uint8_t *)write_buf + transferred, write_size - transferred);
				write_size -= transferred;
			}

			// compute amount of data to write
			empty_space = BUFFER_SIZE - write_size;
			pending = ls_buffer_size(&buf->write_buf);
			if (pending > empty_space)
				pending = empty_space;

			if (pending)
			{
				memcpy((uint8_t *)write_buf + write_size, buf->write_buf.data, pending);
				write_size += pending;

				// shift remaining data to the beginning of the buffer
				remain = ls_buffer_size(&buf->write_buf) - pending;
				memmove(buf->write_buf.data, buf->write_buf.data + pending, remain);
				buf->write_buf.pos = buf->write_buf.end = buf->write_buf.data + remain;
			}

			if (write_size)
			{
				// queue the next write
				rc = ls_aio_write(buf->write_aio, 0, write_buf, write_size, NULL, NULL);
				if (rc == -1)
				{
					buf->should_run = 0;
					break;
				}
			}

			break;
		default:
		case LS_AIO_CANCELED:
		case LS_AIO_ERROR:
			lock_lock(&buf->lock);
			buf->should_run = 0;
			break;
		case LS_AIO_PENDING:
			goto retry;
		}
	}

	ls_close(buf->write_aio), buf->write_aio = NULL;

	lock_unlock(&buf->lock);
	return 0;
}

static const struct ls_class IoBufClass = {
	.type = LS_IO_BUFFER,
	.cb = sizeof(struct ls_io_buf),
	.dtor = (ls_dtor_t)ls_io_buf_dtor,
	.wait = NULL
};

ls_handle ls_create_io_buffer(ls_handle fh, int access)
{
	struct ls_file *fp;
	struct ls_io_buf *buf;

	fp = ls_resolve_file(fh);
	if (!fp)
		return NULL;

	if (!fp->is_async)
	{
		ls_set_errno(LS_INVALID_HANDLE);
		return NULL;
	}

	// must be read, write, or both
	if (!(access & (LS_FILE_READ | LS_FILE_WRITE)))
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	buf = ls_handle_create(&IoBufClass);
	if (!buf)
		return NULL;

	buf->fh = fh;

	lock_init(&buf->lock);
	cond_init(&buf->cond);

	if (access & LS_FILE_READ)
	{
		buf->read_aio = ls_aio_open(fh);
		buf->read_thread = ls_thread_create((ls_thread_func_t)&ls_io_buf_read_worker, buf);
	}

	if (access & LS_FILE_WRITE)
	{
		buf->write_aio = ls_aio_open(fh);
		buf->write_thread = ls_thread_create((ls_thread_func_t)&ls_io_buf_write_worker, buf);
	}

	return buf;
}

size_t ls_io_buffer_read(ls_handle ioh, void *buffer, size_t size)
{
	struct ls_io_buf *buf;
	size_t avail;
	size_t remain;

	if (!buffer || !size)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	buf = ioh;
	if (!buf)
		return ls_set_errno(LS_INVALID_HANDLE);

	if ((buf->access & LS_FILE_READ) == 0)
		return ls_set_errno(LS_INVALID_HANDLE);

	lock_lock(&buf->lock);

	avail = ls_buffer_size(&buf->read_buf);
	if (size > avail)
		size = avail;

	if (size)
	{
		remain = avail - size;

		memcpy(buffer, &buf->read_buf.data, size);

		// shift remaining data to the beginning of the buffer
		memmove(&buf->read_buf.data, &buf->read_buf.data[size], remain);
		buf->read_buf.pos = buf->read_buf.end = buf->read_buf.data + remain;
	}

	lock_unlock(&buf->lock);

	return size;
}

size_t ls_io_buffer_write(ls_handle ioh, const void *buffer, size_t size)
{
	struct ls_io_buf *buf;
	int rc;

	if (!buffer || !size)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	buf = ioh;
	if (!buf)
		return ls_set_errno(LS_INVALID_HANDLE);

	if ((buf->access & LS_FILE_WRITE) == 0)
		return ls_set_errno(LS_INVALID_HANDLE);

	lock_lock(&buf->lock);

	rc = ls_buffer_write(&buf->write_buf, buffer, size);
	if (rc == -1)
		rc = _ls_errno;

	cond_signal(&buf->cond);

	lock_unlock(&buf->lock);

	return ls_set_errno(rc);
}

int ls_io_buffer_flush(ls_handle ioh)
{
	struct ls_io_buf *buf;

	buf = ioh;
	if (!buf)
		return ls_set_errno(LS_INVALID_HANDLE);

	if ((buf->access & LS_FILE_WRITE) == 0)
		return ls_set_errno(LS_INVALID_HANDLE);

	lock_lock(&buf->lock);
	buf->do_flush = 1;
	cond_signal(&buf->cond);
	lock_unlock(&buf->lock);

	return 0;
}
