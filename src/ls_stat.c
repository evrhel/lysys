#include <lysys/ls_stat.h>

#include <lysys/ls_core.h>
#include <lysys/ls_memory.h>
#include <lysys/ls_file.h>

#include <stdlib.h>
#include <memory.h>

#include "ls_handle.h"
#include "ls_native.h"

#if !LS_WINDOWS
int type_from_unix_flags(int flags)
{
    switch (flags & S_IFMT)
    {
    case S_IFREG:
        return LS_FT_FILE;
    case S_IFDIR:
        return LS_FT_DIR;
    case S_IFLNK:
        return LS_FT_LINK;
    case S_IFIFO:
        return LS_FT_PIPE;
    case S_IFSOCK:
        return LS_FT_SOCK;
    default:
        return LS_FT_UNKNOWN;
    }
}
#endif

int ls_stat(const char *path, struct ls_stat *st)
{
#if LS_WINDOWS
	BOOL bRet;
	WIN32_FILE_ATTRIBUTE_DATA fad;
	WCHAR szPath[MAX_PATH];

	ls_utf8_to_wchar_buf(path, szPath, MAX_PATH);
	bRet = GetFileAttributesExW(szPath, GetFileExInfoStandard, &fad);	
	if (!bRet) return -1;

	st->size = ((uint64_t)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;

	st->type = LS_FT_FILE;
	if (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		st->type = LS_FT_DIR;
	else if (fad.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
		st->type = LS_FT_LINK;
	else if (fad.dwFileAttributes & FILE_ATTRIBUTE_DEVICE)
		st->type = LS_FT_DEV;

	return 0;
#else
    struct stat pst;
    int rc;
    
    rc = stat(path, &pst);
    if (rc == -1)
        return -1;
    
    st->size = pst.st_size;
    st->type = type_from_unix_flags(pst.st_flags);
    
    return 0;
#endif // LS_WINDOWS
}

int ls_fstat(ls_handle file, struct ls_stat *st)
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
#else
    struct stat pst;
    int rc;
    
    rc = fstat(file, &pst);
    if (rc == -1)
        return -1;
    
    st->size = pst.st_size;
    st->type = type_from_unix_flags(pst.st_flags);
    
    return 0;
#endif // LS_WINDOWS
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
#else
    int pmode;
    int rc;
    
    switch (mode) {
    case LS_A_READ:
        pmode = R_OK;
        break;
    case LS_A_WRITE:
        pmode = W_OK;
        break;
    case LS_A_EXECUTE:
        pmode = X_OK;
        break;
    case LS_A_EXIST:
        pmode = F_OK;
        break;
    default:
        return -1;
    }
    
    return access(path, pmode);
#endif // LS_WINDOWS
}

struct ls_dir_data
{
#if LS_WINDOWS
	struct ls_dir dir;
	char name[MAX_PATH];

	WIN32_FIND_DATAW fd;
	HANDLE hFind;
#else
    struct ls_dir dir;
    struct dirent dirent;
    DIR *unidir;
#endif // LS_WINDOWS
};

static void LS_CLASS_FN ls_dir_data_dtor(struct ls_dir_data *data)
{
#if LS_WINDOWS
	if (data->hFind != INVALID_HANDLE_VALUE)
		FindClose(data->hFind);
#else
    if (data->unidir)
        closedir(data->unidir);
#endif // LS_WINDOWS
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
	len--; // remove the null terminator

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
#else
    struct ls_dir_data *data;
    
    data = ls_handle_create(&DirClass);
    
    data->unidir = opendir(path);
    if (!data->unidir)
    {
        ls_close(data);
        return NULL;
    }
    
    return data;
#endif // LS_WINDOWS
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
#else
    struct ls_dir_data *data = dir;
    struct dirent *dirent;
    struct stat st;
    int rc;
    
    dirent = readdir(data->unidir);
    if (!dirent)
        return NULL;
    
    memcpy(&data->dirent, dirent, sizeof(struct dirent));
    
    data->dir.name = data->dirent.d_name;
    
    switch (data->dirent.d_type)
    {
    case DT_REG:
        data->dir.type = LS_FT_FILE;
        break;
    case DT_DIR:
        data->dir.type = LS_FT_DIR;
        break;
    case DT_LNK:
        data->dir.type = LS_FT_LINK;
        break;
    case DT_SOCK:
        data->dir.type = LS_FT_SOCK;
        break;
    default:
        data->dir.type = LS_FT_UNKNOWN;
        break;
    }
    
    data->dir.size = 0;
    
    rc = stat(data->dir.name, &st);
    if (rc == 0)
        data->dir.size = st.st_size;
    
    return &data->dir;
#endif // LS_WINDOWS
}
