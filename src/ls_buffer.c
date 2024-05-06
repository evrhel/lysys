#include "ls_buffer.h"

#include <stdlib.h>
#include <memory.h>

#include <lysys/ls_core.h>

#include "ls_native.h"

void ls_buffer_release(ls_buffer_t *buffer)
{
	ls_free(buffer->data);
	buffer->data = buffer->pos = buffer->end = NULL;
}

void ls_buffer_clear(ls_buffer_t *buffer)
{
	buffer->pos = buffer->data;
}

int ls_buffer_resize(ls_buffer_t *buffer, size_t size)
{
	int rc;

	if (buffer->data + size > buffer->end)
	{
		rc = ls_buffer_reserve(buffer, size);
		if (rc == -1) return -1;
	}

	buffer->pos = buffer->data + size;
	return 0;
}

int ls_buffer_reserve(ls_buffer_t *buffer, size_t capacity)
{
	uint8_t *data;

	data = (uint8_t *)ls_realloc(buffer->data, capacity);	
	if (!data)
		return -1;

	buffer->pos = data + (buffer->pos - buffer->data);
	buffer->end = data + capacity;

	if (buffer->pos > buffer->end)
		buffer->pos = buffer->end;

	buffer->data = data;

	return 0;
}

int ls_buffer_write(ls_buffer_t *buffer, const void *data, size_t size)
{
	int rc;
	uint8_t *npos;

	npos = buffer->pos + size;
	if (npos > buffer->end)
	{
		rc = ls_buffer_reserve(buffer, npos - buffer->data);
		if (rc == -1) return -1;
	}

	memcpy(buffer->pos, data, size);

	buffer->pos += size; // cannot set as data may have been reallocated
	return 0;
}

int ls_buffer_put_char(ls_buffer_t *buffer, char c)
{
	return ls_buffer_write(buffer, &c, 1);
}

int ls_buffer_put_wchar(ls_buffer_t *buffer, wchar_t c)
{
	return ls_buffer_write(buffer, (uint8_t *)&c, sizeof(wchar_t));
}
