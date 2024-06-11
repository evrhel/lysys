#include "ls_file_priv.h"

#include <lysys/ls_file.h>

#include "ls_handle.h"

static LS_THREADLOCAL ls_file_t _file = { 0 };

ls_file_t *ls_resolve_file(ls_handle fh, int *flags)
{
	struct ls_handle_info *info;

	switch ((intptr_t)fh)
	{
	case 0:
		ls_set_errno(LS_INVALID_HANDLE);
		return NULL;
	case (intptr_t)LS_DEVNULL:
#if LS_WINDOWS
		_file.hFile = NULL;
#else
		_file.fd = -1;
#endif // LS_WINDOWS
		*flags = LS_FILE_READ | LS_FILE_WRITE;
		break;
	case (intptr_t)LS_STDIN:
#if LS_WINDOWS
		_file.hFile = GetStdHandle(STD_INPUT_HANDLE);
#else
		_file.fd = 0;
#endif // LS_WINDOWS
		*flags = LS_FILE_READ;
		break;
	case (intptr_t)LS_STDOUT:
#if LS_WINDOWS
		_file.hFile = GetStdHandle(STD_OUTPUT_HANDLE);
#else
		_file.fd = 1;
#endif // LS_WINDOWS
		*flags = LS_FILE_WRITE;
		break;
	case (intptr_t)LS_STDERR:
#if LS_WINDOWS
		_file.hFile = GetStdHandle(STD_ERROR_HANDLE);
#else
		_file.fd = 2;
#endif // LS_WINDOWS
		*flags = LS_FILE_WRITE;
		break;
	default:
		if (LS_IS_PSUEDO_HANDLE(fh))
		{
			ls_set_errno(LS_INVALID_HANDLE);
			return NULL;
		}

		info = LS_HANDLE_INFO(fh);
		if (info->clazz->type & LS_IO_STREAM)
		{
			*flags = info->flags;
			return fh;
		}

		ls_set_errno(LS_INVALID_HANDLE);
		return NULL;
	}

	return &_file;
}

ls_pipe_t *ls_resolve_pipe(ls_handle fh, int *flags)
{
	struct ls_handle_info *info;

	if (LS_IS_PSUEDO_HANDLE(fh))
	{
		ls_set_errno(LS_INVALID_HANDLE);
		return NULL;
	}

	info = LS_HANDLE_INFO(fh);
	if (info->clazz->type == LS_PIPE)
		return fh;

	ls_set_errno(LS_INVALID_HANDLE);
	return NULL;
}
