#include "ls_file_priv.h"

#include <lysys/ls_file.h>

struct ls_file *ls_resolve_file(ls_handle fh)
{
	static LS_THREADLOCAL struct ls_file file = { 0 };

#if LS_WINDOWS
	switch ((intptr_t)fh)
	{
	case -1:
	case 0:
		ls_set_errno(LS_INVALID_HANDLE);
		return NULL;
	case LS_STDIN:
		file.hFile = GetStdHandle(STD_INPUT_HANDLE);
		file.is_async = 0;
		break;
	case LS_STDOUT:
		file.hFile = GetStdHandle(STD_OUTPUT_HANDLE);
		file.is_async = 0;
		break;
	case LS_STDERR:
		file.hFile = GetStdHandle(STD_ERROR_HANDLE);
		file.is_async = 0;
		break;
	default:
		return (struct ls_file *)fh;
	}
#else
	switch ((intptr_t)fh)
	{
	case -1:
	case 0:
		ls_set_errno(LS_INVALID_HANDLE);
		return NULL;
	case LS_STDIN:
		file.fd = 0;
		file.is_async = 0;
		break;
	case LS_STDOUT:
		file.fd = 1;
		file.is_async = 0;
		break;
	case LS_STDERR:
		file.fd = 2;
		file.is_async = 0;
		break;
	default:
		return (struct ls_file *)fh;
	}
#endif // LS_WINDOWS

	return &file;
}
