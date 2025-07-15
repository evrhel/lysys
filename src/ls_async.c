#include <lysys/ls_async.h>
#include <lysys/ls_thread.h>
#include <lysys/ls_core.h>
#include <lysys/ls_sync.h>

#include "ls_handle.h"
#include "ls_sync_util.h"

#define THREAD_COUNT 4

#define TASK_POOL_SIZE 128
#define TASK_INDEX_MASK 0xffff
#define TASK_UUID_MASK 0xffffffff00000000

struct task
{
	int used;
	int initialized;
	int status;
	int is_canceled;
	int auto_free;
	unsigned long thread_id;
	ls_atom atom;
	ls_task_completion_fn_t cb;
	uintptr_t param;
	void *result;

	ls_lock_t lock;
	ls_cond_t cond;
};

static int _started = 0;
static ls_lock_t _lock;
static ls_handle _sema = NULL;
static struct task *_head = NULL;
static struct task *_tail = NULL;

static struct task _task_pool[TASK_POOL_SIZE];
static size_t _next_free = 0;
static ls_atom _next_id = 0;

static struct task *atom_to_task(ls_atom atom)
{
	struct task *task;
	size_t index;

	index = (atom & TASK_UUID_MASK) - 1;
	if (index >= TASK_POOL_SIZE)
	{
		ls_set_errno(LS_NOT_FOUND);
		return NULL;
	}

	task = _task_pool + index;
	if (task->atom != atom)
	{
		ls_set_errno(LS_NOT_FOUND);
		return NULL;
	}

	return task;
}

static struct task *task_acquire(ls_atom atom)
{
	struct task *task;

	lock_lock(&_lock);
	task = atom_to_task(atom);
	if (!task)
	{
		lock_unlock(&_lock);
		return task;
	}

	lock_lock(&task->lock);
	lock_unlock(&_lock);

	if (!task->used)
	{
		lock_unlock(&task->lock);
		return NULL;
	}

	return task;
}

static void task_release(struct task *task)
{
	lock_unlock(&task->lock);
}

static struct task *task_alloc(void)
{
	struct task *tp;

	if (_next_free == TASK_POOL_SIZE)
		return NULL;

	tp = _task_pool + _next_free;

	if (!tp->initialized)
	{
		if (lock_init(&tp->lock))
			return NULL;

		if (cond_init(&tp->cond))
		{
			lock_destroy(&tp->lock);
			return NULL;
		}
	}

	tp->used = 1;
	tp->initialized = 1;
	tp->status = LS_TASK_STATUS_IN_PROGRESS;
	tp->is_canceled = 0;
	tp->auto_free = 0;
	tp->thread_id = ls_thread_id_self();

	tp->atom = tp - _task_pool + 1;
	tp->atom |= _next_id << 32;

	for (; _next_free < TASK_POOL_SIZE; ++_next_free)
	{
		if (!_task_pool[_next_free].used)
			break;
	}

	return tp;
}

static void task_free(struct task *task)
{
	size_t index;

	task->used = 0;
	task->status = 0;
	task->is_canceled = 0;
	task->thread_id = 0;
	task->atom = 0;
	task->cb = NULL;
	task->param = 0;
	task->result = NULL;

	index = task - _task_pool;
	if (index < _next_free)
		_next_free = index;
}

static int ls_async_thread(void *unused)
{
	int rc;
	struct task *tp;

	for (;;)
	{
		rc = ls_wait(_sema);
		if (rc != 0)
		{
			ls_perror("ls_wait");
			abort();
		}

		lock_lock(&_lock);
		for (tp = _task_pool; tp < _task_pool + TASK_POOL_SIZE; ++tp)
		{
			if (tp->used && tp->status == LS_TASK_STATUS_IN_PROGRESS)
				break;
		}
		lock_unlock(&_lock);

		if (tp == _task_pool + TASK_POOL_SIZE)
			continue; // nothing available

		lock_lock(&tp->lock);
		if (tp->is_canceled)
		{
			tp->status = LS_TASK_STATUS_CANCELED;
			cond_broadcast(&tp->cond);
			lock_unlock(&tp->lock);
			continue;
		}
		lock_unlock(&tp->lock);

		rc = tp->cb(tp->param, &tp->result);

		lock_lock(&tp->lock);
		tp->status = rc == 0 ? LS_TASK_STATUS_COMPLETE : LS_TASK_STATUS_ERROR;
		cond_broadcast(&tp->cond);

		if (tp->auto_free)
		{
			lock_lock(&_lock);
			task_free(tp);
			lock_unlock(&_lock);
		}

		lock_unlock(&tp->lock);
	}
}

int ls_async_init(void)
{
	ls_handle thread;
	int i;

	if (_started)
		return ls_set_errno(LS_INVALID_STATE);

	_started = 1;

	_head = NULL;
	_tail = NULL;
	memset(&_task_pool, 0, sizeof(_task_pool));
	_next_free = 0;

	_sema = ls_semaphore_create(0);
	if (!_sema)
	{
		_started = 0;
		return -1;
	}

	if (lock_init(&_lock))
	{
		ls_close(_sema), _sema = NULL;
		_started = 0;
		return -1;
	}

	for (i = 0; i < THREAD_COUNT; ++i)
	{
		thread = ls_thread_create(&ls_async_thread, NULL);
		if (!thread)
		{
			ls_perror("ls_thread_create");
			abort();
		}

		ls_close(thread);
	}

	return 0;
}

ls_atom ls_dispatch(ls_task_completion_fn_t fn, uintptr_t param)
{
	struct task *task;

	lock_lock(&_lock);
	task = task_alloc();
	lock_unlock(&_lock);

	if (!task)
		return 0;

	return task->atom;
}

int ls_get_async_status(ls_atom task_atom, void **result)
{
	struct task *task;
	int status;

	task = task_acquire(task_atom);
	if (!task)
		return -1;

	status = task->status;
	if (status == LS_TASK_STATUS_COMPLETE && result)
		*result = task->result;

	task_release(task);

	return status;
}

int ls_await(ls_atom task_atom, void **result)
{
	struct task *task;
	int status;

	task = task_acquire(task_atom);
	if (!task)
		return -1;

	if (task->thread_id != ls_thread_id_self())
	{
		task_release(task_atom);
		return ls_set_errno(LS_ACCESS_DENIED);
	}

	if (task->auto_free)
	{
		task_release(task_atom);
		return ls_set_errno(LS_NOT_WAITABLE);
	}

	while (task->status == LS_TASK_STATUS_IN_PROGRESS)
		cond_wait(&task->cond, &task->lock, LS_INFINITE);

	status = task->status;
	if (result && status == LS_TASK_STATUS_COMPLETE)
		*result = task->result;

	lock_lock(&_lock);
	task_free(task);
	lock_unlock(&_lock);

	task_release(task);

	return status;
}

int ls_ignore(ls_atom task_atom)
{
	struct task *task;

	task = task_acquire(task_atom);
	if (!task)
		return -1;

	if (task->thread_id != ls_thread_id_self())
	{
		task_release(task_atom);
		return ls_set_errno(LS_ACCESS_DENIED);
	}

	if (task->status != LS_TASK_STATUS_IN_PROGRESS)
	{
		lock_lock(&_lock);
		task_free(task);
		lock_unlock(&_lock);

		task_release(task);

		return 0;
	}

	task->auto_free = 1;

	task_release(task);
	
	return 0;
}

int ls_cancel(ls_atom task_atom)
{
	struct task *task;

	task = task_acquire(task_atom);
	if (!task)
		return -1;

	if (task->status != LS_TASK_STATUS_IN_PROGRESS)
	{
		task_release(task);
		return 0;
	}

	if (task->auto_free)
	{
		task_release(task_atom);
		return ls_set_errno(LS_INVALID_STATE);
	}

	task->is_canceled = 1;
	cond_signal(&task->cond);

	task_release(task);
		
	return 0;
}
