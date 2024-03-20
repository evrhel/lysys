#include <lysys/ls_core.h>

#include <stdlib.h>
#include <memory.h>
#include <signal.h>
#include <string.h>

#include <lysys/ls_time.h>

static ls_exit_hook_t *_exit_hooks = NULL;
static size_t _num_exit_hooks = 0;

static struct ls_allocator _allocator = {0};

void ls_init(void)
{
	_exit_hooks = NULL;
	_num_exit_hooks = 0;

	ls_set_allocator(NULL);

	ls_set_epoch();
}

void ls_shutdown(void)
{
	ls_free(_exit_hooks);
	_exit_hooks = NULL;
	_num_exit_hooks = 0;
	memset(&_allocator, 0, sizeof(_allocator));
}

void ls_set_allocator(const struct ls_allocator *allocator)
{
	if (allocator == NULL)
	{
		_allocator = (struct ls_allocator) {
			.malloc = malloc,
			.calloc = calloc,
			.realloc = realloc,
			.free = free
		};
	}
	else
		_allocator = *allocator;
}

int ls_add_exit_hook(ls_exit_hook_t hook)
{
	ls_exit_hook_t *hooks;

	hooks = ls_realloc(_exit_hooks, (_num_exit_hooks + 1) * sizeof(ls_exit_hook_t));
	if (hooks == NULL) return -1;

	_exit_hooks = hooks;
	_exit_hooks[_num_exit_hooks++] = hook;

	return 0;
}

void ls_exit(int status)
{
	ls_exit_hook_t *hook;

	for (hook = _exit_hooks; hook < _exit_hooks + _num_exit_hooks; ++hook)
		(*hook)(status);
	
	ls_shutdown();
	exit(status);
}

void *ls_malloc(size_t size)
{
	void *ptr;
	ptr = _allocator.malloc(size);
	if (!ptr) abort();
	return ptr;
}

void *ls_calloc(size_t nmemb, size_t size)
{
	void *ptr;
	ptr = _allocator.calloc(nmemb, size);
	if (!ptr) abort();
	return ptr;
}

void *ls_realloc(void *ptr, size_t size)
{
	ptr = _allocator.realloc(ptr, size);
	if (!ptr) abort();
	return ptr;
}

void ls_free(void *ptr)
{
	_allocator.free(ptr);
}

char *ls_strdup(const char *s)
{
	size_t len;
	char *dup;

	len = strlen(s);
	dup = ls_malloc(len + 1);
	memcpy(dup, s, len + 1);
	return dup;
}

size_t ls_substr(const char *s, size_t n, char *buf, size_t size)
{
	if (size == 0) return 0;

	if (n > size - 1)
		n = size - 1;
	memcpy(buf, s, n);
	buf[n] = 0;
	return n;
}
