#include <lysys/ls_stat.h>

#include <lysys/ls_core.h>
#include <lysys/ls_memory.h>
#include <lysys/ls_file.h>

#include <stdlib.h>

#include "ls_handle.h"
#include "ls_native.h"

int ls_stat(const char *path, struct ls_stat *stat)
{
#if LS_WINDOWS
	BOOL bRet;
	WIN32_FILE_ATTRIBUTE_DATA fad;
	WCHAR szPath[MAX_PATH];

	ls_utf8_to_wchar_buf(path, szPath, MAX_PATH);
	bRet = GetFileAttributesExW(szPath, GetFileExInfoStandard, &fad);	
	if (!bRet) return -1;

	stat->size = ((uint64_t)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;

	stat->type = LS_FT_FILE;
	if (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		stat->type = LS_FT_DIR;
	else if (fad.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
		stat->type = LS_FT_LINK;
	else if (fad.dwFileAttributes & FILE_ATTRIBUTE_DEVICE)
		stat->type = LS_FT_DEV;

	return 0;
#endif
}

int ls_fstat(ls_handle file, struct ls_stat *stat)
{
#if LS_WINDOWS
	BOOL bRet;
	BY_HANDLE_FILE_INFORMATION fi;

	bRet = GetFileInformationByHandle(file, &fi);
	if (!bRet) return -1;

	stat->size = ((uint64_t)fi.nFileSizeHigh << 32) | fi.nFileSizeLow;

	stat->type = LS_FT_FILE;
	if (fi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		stat->type = LS_FT_DIR;
	else if (fi.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
		stat->type = LS_FT_LINK;
	else if (fi.dwFileAttributes & FILE_ATTRIBUTE_DEVICE)
		stat->type = LS_FT_DEV;

	return 0;
#endif
}

int ls_access(const char *path, int mode)
{
#if LS_WINDOWS
	WCHAR szPath[MAX_PATH];
	DWORD dwAccess;
	DWORD dwRet;

	ls_utf8_to_wchar_buf(path, szPath, MAX_PATH);

	dwAccess = ls_get_access_rights(mode);
	dwRet = GetFileAttributesW(szPath);
	if (dwRet == INVALID_FILE_ATTRIBUTES) return -1;

	if (dwAccess & LS_A_WRITE)
	{
		if (dwRet & FILE_ATTRIBUTE_READONLY)
			return -1;
	}

	return 0;
#endif
}

struct ls_dir_data
{
#if LS_WINDOWS
	struct ls_dir dir;
	char name[MAX_PATH];

	WIN32_FIND_DATAW fd;
	HANDLE hFind;
#endif
};

static void LS_CLASS_FN ls_dir_data_dtor(struct ls_dir_data *data)
{
#if LS_WINDOWS
	if (data->hFind != INVALID_HANDLE_VALUE)
		FindClose(data->hFind);
#endif
}

static struct ls_class DirClass = {
	.type = LS_DIR,
	.cb = sizeof(struct ls_dir_data),
	.dtor = (ls_dtor_t)&ls_dir_data_dtor,
	.wait = NULL
};

ls_handle ls_opendir(const char *path)
{
#if LS_WINDOWS
	WCHAR szPath[MAX_PATH + 4]; // +4 for "/*\0"
	int len;
	struct ls_dir_data *data;

	len = ls_utf8_to_wchar_buf(path, szPath, MAX_PATH);
	if (!len) return NULL;

	if (szPath[len - 1] != L'\\')
		szPath[len++] = L'\\';
	szPath[len++] = L'*';
	szPath[len] = L'\0';

	data = ls_handle_create(&DirClass);
	if (!data) return NULL;

	data->hFind = FindFirstFileW(szPath, &data->fd);
	if (data->hFind == INVALID_HANDLE_VALUE)
	{
		ls_close(data);
		return NULL;
	}
	
	ls_wchar_to_utf8_buf(data->fd.cFileName, data->name, MAX_PATH);
	data->dir.name = data->name;

	data->dir.type = LS_FT_FILE;
	if (data->fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		data->dir.type = LS_FT_DIR;
	else if (data->fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
		data->dir.type = LS_FT_LINK;
	else if (data->fd.dwFileAttributes & FILE_ATTRIBUTE_DEVICE)
		data->dir.type = LS_FT_DEV;

	data->dir.size = ((uint64_t)data->fd.nFileSizeHigh << 32) | data->fd.nFileSizeLow;

	return data;
#endif
}

struct ls_dir *ls_readdir(ls_handle dir)
{
#if LS_WINDOWS
	struct ls_dir_data *data = (struct ls_dir_data *)dir;
	BOOL bRet;

	if (data->hFind == INVALID_HANDLE_VALUE) return NULL;

	ls_wchar_to_utf8_buf(data->fd.cFileName, data->name, MAX_PATH);
	data->dir.name = data->name;

	data->dir.type = LS_FT_FILE;
	if (data->fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		data->dir.type = LS_FT_DIR;
	else if (data->fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
		data->dir.type = LS_FT_LINK;
	else if (data->fd.dwFileAttributes & FILE_ATTRIBUTE_DEVICE)
		data->dir.type = LS_FT_DEV;

	data->dir.size = ((uint64_t)data->fd.nFileSizeHigh << 32) | data->fd.nFileSizeLow;

	bRet = FindNextFileW(data->hFind, &data->fd);
	if (!bRet)
	{
		FindClose(data->hFind);
		data->hFind = INVALID_HANDLE_VALUE;
	}

	return &data->dir;
#endif
}
