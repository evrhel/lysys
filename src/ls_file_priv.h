#ifndef _LS_FILE_PRIV_H_
#define _LS_FILE_PRIV_H_

#include "ls_native.h"

struct ls_file
{
#if LS_WINDOWS
	HANDLE hFile;
	OVERLAPPED ov; // used with pipes
#else
	int fd;
#endif // LS_WINDOWS

	int is_async;
	int connected;
};

struct ls_file *ls_resolve_file(ls_handle fh);

#endif // _LS_FILE_PRIV_H_
