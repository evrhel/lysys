#include "ls_native.h"

#include <stdlib.h>

#include <lysys/ls_memory.h>
#include <lysys/ls_file.h>
#include <lysys/ls_core.h>
#include <lysys/ls_proc.h>

#include "ls_buffer.h"

#ifdef LS_WINDOWS

int ls_utf8_to_wchar_buf(const char *utf8, LPWSTR buf, int cbbuf)
{
	return MultiByteToWideChar(CP_UTF8, 0, utf8, -1, buf, cbbuf);
}

LPWSTR ls_utf8_to_wchar(const char *utf8)
{
	LPWSTR wstr;
	int len;

	len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
	if (!len) return NULL;

	wstr = ls_malloc(len * sizeof(WCHAR));
	len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wstr, len);
	if (!len)
	{
		ls_free(wstr);
		return NULL;
	}

	return wstr;
}

int ls_wchar_to_utf8_buf(LPCWSTR wstr, char *buf, int cbbuf)
{
	return WideCharToMultiByte(CP_UTF8, 0, wstr, -1, buf, cbbuf, NULL, NULL);
}

char *ls_wchar_to_utf8(LPCWSTR wstr)
{
	char *utf8;
	int len;

	len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
	if (!len) return NULL;

	utf8 = ls_malloc(len);
	len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, utf8, len, NULL, NULL);
	if (!len)
	{
		ls_free(utf8);
		return NULL;
	}

	return utf8;
}

DWORD ls_protect_to_flags(int protect)
{
	DWORD flProtect = 0;

	if (protect & LS_PROT_READ)
		flProtect |= PAGE_READONLY;

	if (protect & LS_PROT_WRITE)
		flProtect |= PAGE_READWRITE;

	if (protect & LS_PROT_WRITECOPY)
		flProtect |= PAGE_WRITECOPY;

	if (protect & LS_PROT_EXEC)
		flProtect |= PAGE_EXECUTE;

	return flProtect;
}

int ls_flags_to_protect(DWORD flProtect)
{
	int protect = 0;

	if (flProtect & PAGE_READONLY)
		protect |= LS_PROT_READ;

	if (flProtect & PAGE_READWRITE)
		protect |= LS_PROT_WRITE;

	if (flProtect & PAGE_WRITECOPY)
		protect |= LS_PROT_WRITECOPY;

	if (flProtect & PAGE_EXECUTE)
		protect |= LS_PROT_EXEC;

	return protect;
}

DWORD ls_get_access_rights(int access)
{
	DWORD dwDesiredAccess = 0;

	if (access & LS_A_READ)
		dwDesiredAccess |= GENERIC_READ;

	if (access & LS_A_WRITE)
		dwDesiredAccess |= GENERIC_WRITE;

	if (access & LS_A_EXECUTE)
		dwDesiredAccess |= GENERIC_EXECUTE;

	return dwDesiredAccess;
}

DWORD ls_get_flags_and_attributes(int access)
{
	DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;

	if (access & LS_A_ASYNC)
		dwFlagsAndAttributes |= FILE_FLAG_OVERLAPPED;

	if (access & LS_A_RANDOM)
		dwFlagsAndAttributes |= FILE_FLAG_RANDOM_ACCESS;

	if (access & LS_A_SEQUENTIAL)
		dwFlagsAndAttributes |= FILE_FLAG_SEQUENTIAL_SCAN;

	return dwFlagsAndAttributes;
}

int ls_append_escaped(struct ls_buffer *buf, LPWSTR str)
{
	WCHAR c;

	while (*str)
	{
		c = *str;
		switch (c)
		{
		case '\"':
		case '\\':
			if (ls_buffer_put_wchar(buf, L'\\') == -1) return -1;
			if (ls_buffer_put_wchar(buf, c) == -1) return -1;
			break;
		case '\t':
			if (ls_buffer_put_wchar(buf, L'\\') == -1) return -1;
			if (ls_buffer_put_wchar(buf, L't') == -1) return -1;
			break;
		case '\n':
			if (ls_buffer_put_wchar(buf, L'\\') == -1) return -1;
			if (ls_buffer_put_wchar(buf, L'n') == -1) return -1;
			break;
		case '\r':
			if (ls_buffer_put_wchar(buf, L'\\') == -1) return -1;
			if (ls_buffer_put_wchar(buf, L'r') == -1) return -1;
			break;
		default:
			if (ls_buffer_put_wchar(buf, c) == -1) return -1;
			break;
		}
	}

	return 0;
}

LPWSTR ls_build_command_line(const char *path, const char *argv[])
{
	LPWSTR wpath;
	LPWSTR wargv[LS_MAX_ARGV];
	LPWSTR *warg;
	int num_args = 0;
	struct ls_buffer buf;
	size_t len;
	int rc = 0;

	ZeroMemory(wargv, sizeof(wargv));
	ZeroMemory(&buf, sizeof(buf));

	// convert path to wide char
	wpath = ls_utf8_to_wchar(path);
	if (!wpath) goto done;
	len = wcslen(wpath) + 1; // 1 for null terminator

	// convert argv to wide char
	if (argv)
	{
		while (*argv)
		{
			if (num_args >= LS_MAX_ARGV) goto done;

			wargv[num_args] = ls_utf8_to_wchar(*argv);
			if (!wargv[num_args]) goto done;

			len += wcslen(wargv[num_args]) + 3; // 3 for space and quotes

			num_args++;
			argv++;
		}
	}
	
	// we will need at least len * sizeof(WCHAR) bytes
	ls_buffer_reserve(&buf, len * sizeof(WCHAR));

	// append path
	rc = ls_append_escaped(&buf, wpath);

	// append args
	for (warg = wargv; warg < wargv + num_args; warg++)
	{
		rc = ls_buffer_put_wchar(&buf, L' ');
		if (rc == -1) goto done;

		rc = ls_buffer_put_wchar(&buf, L'\"');
		if (rc == -1) goto done;

		rc = ls_append_escaped(&buf, *warg);
		if (rc == -1) goto done;

		rc = ls_buffer_put_wchar(&buf, L'\"');
		if (rc == -1) goto done;
	}

	// null terminator
	rc = ls_buffer_put_wchar(&buf, L'\0');

done:
	if (rc == -1)
		ls_buffer_release(&buf);

	while (num_args > 1)
		ls_free(wargv[--num_args]);

	ls_free(wpath);
	return (LPWSTR)buf.data;
}

LPWSTR ls_build_environment(const char *envp[])
{
	struct ls_buffer buf;
	size_t len;
	int rc;

	LPWSTR str = NULL;

	ZeroMemory(&buf, sizeof(buf));

	if (envp)
	{
		while (*envp)
		{
			str = ls_utf8_to_wchar(*envp);
			if (!str) goto failure;

			len = wcslen(str);

			rc = ls_buffer_write(&buf, str, len * sizeof(WCHAR));
			if (rc == -1) goto failure;

			rc = ls_buffer_put_wchar(&buf, L'\0');
			if (rc == -1) goto failure;

			ls_free(str);

			envp++;
		}
	}

	str = NULL;

	rc = ls_buffer_put_wchar(&buf, L'\0');
	if (rc == -1) goto failure;

	return (LPWSTR)buf.data;

failure:
	ls_free(str);
	ls_buffer_release(&buf);
	return NULL;
}

#endif
