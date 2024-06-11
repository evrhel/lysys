#ifndef _LS_BUFFER_H_
#define _LS_BUFFER_H_

#include <stdint.h>
#include <wchar.h>

typedef struct ls_buffer
{
	uint8_t *data;
	uint8_t *pos;
	uint8_t *end;
} ls_buffer_t;

#define ls_buffer_size(b) ((b)->end - (b)->data)

void ls_buffer_release(ls_buffer_t *buffer);

void ls_buffer_clear(ls_buffer_t *buffer);

int ls_buffer_resize(ls_buffer_t *buffer, size_t size);

int ls_buffer_reserve(ls_buffer_t *buffer, size_t capacity);

int ls_buffer_write(ls_buffer_t *buffer, const void *data, size_t size);

int ls_buffer_put_char(ls_buffer_t *buffer, char c);

int ls_buffer_put_wchar(ls_buffer_t *buffer, wchar_t c);

#endif
