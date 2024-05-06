#include "ls_util.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#include <lysys/ls_core.h>

#include "ls_native.h"

map_t *ls_map_create(map_cmp_t cmp, map_free_t key_free, map_dup_t key_dup,
	map_free_t value_free, map_dup_t value_dup)
{
	map_t *map;

	map = ls_malloc(sizeof(map_t));
	if (!map)
		return NULL;

	map->entries = NULL;
	map->cmp = cmp;
	map->key_free = key_free;
	map->key_dup = key_dup;
	map->value_free = value_free;
	map->value_dup = value_dup;

	return map;
}

void ls_map_destroy(map_t *map)
{
	if (!map)
		return;

	ls_map_clear(map);
	ls_free(map);
}

int ls_map_clear(map_t *map)
{
	entry_t *entry, *next;

	if (!map)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return -1;
	}

	entry = map->entries;
	while (entry)
	{
		next = entry->next;

		if (map->key_free)
			map->key_free(entry->key);
		if (map->value_free)
			map->value_free(entry->value);

		ls_free(entry);

		entry = next;
	}

	map->entries = NULL;
	map->size = 0;

	return 0;
}

entry_t *ls_map_find(map_t *map, any_t key)
{
	entry_t *entry;
	int equal;

	if (!map)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	entry = map->entries;
	while (entry)
	{
		if (map->cmp)
			equal = !map->cmp(key, entry->key);
		else
			equal = key.ptr == entry->key.ptr;

		if (equal)
			return entry;

		entry = entry->next;
	}

	ls_set_errno(LS_NOT_FOUND);
	return NULL;
}

entry_t *ls_map_insert(map_t *map, any_t key, any_t value)
{
	entry_t *entry;
	any_t nkey;
	any_t nval;

	if (!map)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	entry = ls_map_find(map, key);
	if (entry)
	{
		if (entry->value.ptr == value.ptr)
			return entry;

		_ls_errno = 0;
		nval = map->value_dup ? map->value_dup(value) : value;
		if (_ls_errno)
			return NULL;

		if (map->value_free)
			map->value_free(entry->value);

		entry->value = nval;
		return entry;
	}

	entry = ls_malloc(sizeof(entry_t));
	if (!entry)
		return NULL;

	_ls_errno = 0;
	nkey = map->key_dup ? map->key_dup(key) : key;
	if (_ls_errno)
	{
		ls_free(entry);
		return NULL;
	}

	nval = map->value_dup ? map->value_dup(value) : value;
	if (_ls_errno)
	{
		if (map->key_free)
			map->key_free(nkey);
		ls_free(entry);
		return NULL;
	}

	entry->key = nkey;
	entry->value = nval;

	entry->next = map->entries;
	map->entries = entry;

	map->size++;

	return entry;
}

size_t ls_scbprintf(char *str, size_t cb, const char *format, ...)
{
	int len;
	size_t count;
	va_list args;

	if (!str != !cb)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return -1;
	}

	if (!format)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return -1;
	}

	if (cb > INT_MAX)
	{
		ls_set_errno(LS_OUT_OF_RANGE);
		return -1;
	}

	va_start(args, format);
	len = vsnprintf(str, cb, format, args);
	va_end(args);

	if (len < 0)
	{
		ls_set_errno(LS_INTERNAL_ERROR);
		return -1;
	}

	count = (size_t)len + 1;

	if (!str)
		return count;

	if (cb < count)
	{
		ls_set_errno(LS_BUFFER_TOO_SMALL);
		return -1;
	}

	return count - 1;
}

size_t ls_scbwprintf(wchar_t *str, size_t cb, const wchar_t *format, ...)
{
	int len;
	size_t count;
	va_list args;

	if (!str != !cb)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return -1;
	}

	if (cb > INT_MAX)
	{
		ls_set_errno(LS_OUT_OF_RANGE);
		return -1;
	}

	va_start(args, format);
	len = vswprintf(str, cb / sizeof(wchar_t), format, args);
	va_end(args);

	if (len < 0)
	{
		ls_set_errno(LS_INTERNAL_ERROR);
		return -1;
	}

	count = (size_t)len + 1;
	count *= sizeof(wchar_t);

	if (!str)
		return count;

	if (cb < count)
	{
		ls_set_errno(LS_BUFFER_TOO_SMALL);
		return -1;
	}

	return count - sizeof(wchar_t);
}

size_t ls_strcbcpy(char *dest, const char *src, size_t cb)
{
	size_t len;

	if (!dest || !src)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	len = strlen(src) + 1;
	if (len > cb)
		return ls_set_errno(LS_BUFFER_TOO_SMALL);

	memcpy(dest, src, len);
	return len-1;
}

size_t ls_strcbcat(char *dest, const char *src, size_t cb)
{
	size_t dest_len, src_len;
	size_t len;

	if (!dest || !src)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	dest_len = strlen(dest)+1;
	assert(dest_len < cb);

	src_len = strlen(src);

	len = dest_len + src_len;
	if (len > cb)
		return ls_set_errno(LS_BUFFER_TOO_SMALL);

	memcpy(dest + dest_len - 1, src, src_len + 1);
	return len-1;
}
