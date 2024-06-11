#include <lysys/ls_stat.h>

#include <lysys/ls_core.h>
#include <lysys/ls_memory.h>
#include <lysys/ls_file.h>
#include <lysys/ls_shell.h>
#include <lysys/ls_watch.h>
#include <lysys/ls_string.h>

#include <stdlib.h>
#include <memory.h>
#include <assert.h>

#include "ls_handle.h"
#include "ls_native.h"
#include "ls_util.h"
#include "ls_sync_util.h"
#include "ls_file_priv.h"

#if LS_POSIX
int type_from_mode(int mode)
{
	switch (mode & S_IFMT)
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
#endif // LS_POSIX

int ls_stat(const char *path, struct ls_stat *st)
{
#if LS_WINDOWS
	BOOL bRet;
	WIN32_FILE_ATTRIBUTE_DATA fad;
	WCHAR szPath[MAX_PATH];
	ULARGE_INTEGER uli;

	if (!path || !st)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	if (ls_utf8_to_wchar_buf(path, szPath, MAX_PATH) == -1)
		return -1;

	bRet = GetFileAttributesExW(szPath, GetFileExInfoStandard, &fad);
	if (!bRet)
		return ls_set_errno_win32(GetLastError());

	uli.LowPart = fad.nFileSizeLow;
	uli.HighPart = fad.nFileSizeHigh;
	st->size = uli.QuadPart;

	st->type = LS_FT_FILE;
	if (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		st->type = LS_FT_DIR;
	else if (fad.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
		st->type = LS_FT_LINK;
	else if (fad.dwFileAttributes & FILE_ATTRIBUTE_DEVICE)
		st->type = LS_FT_DEV;

	uli.LowPart = fad.ftCreationTime.dwLowDateTime;
	uli.HighPart = fad.ftCreationTime.dwHighDateTime;
	st->ctime = uli.QuadPart;

	uli.LowPart = fad.ftLastAccessTime.dwLowDateTime;
	uli.HighPart = fad.ftLastAccessTime.dwHighDateTime;
	st->atime = uli.QuadPart;

	uli.LowPart = fad.ftLastWriteTime.dwLowDateTime;
	uli.HighPart = fad.ftLastWriteTime.dwHighDateTime;
	st->atime = uli.QuadPart;

	return 0;
#else
	struct stat pst;
	int rc;

	if (!path || !st)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	rc = stat(path, &pst);
	if (rc == -1)
		return ls_set_errno(ls_errno_to_error(errno));

	st->size = pst.st_size;
	st->type = type_from_mode(pst.st_mode);

	return 0;
#endif // LS_WINDOWS
}

int ls_fstat(ls_handle file, struct ls_stat *st)
{
#if LS_WINDOWS
	BOOL bRet;
	BY_HANDLE_FILE_INFORMATION fi;
	ULARGE_INTEGER uli;
	ls_file_t *pf;
	int flags;

	pf = ls_resolve_file(file, &flags);
	if (!pf)
		return -1;

	if (!st)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	bRet = GetFileInformationByHandle(pf->hFile, &fi);
	if (!bRet)
		return ls_set_errno_win32(GetLastError());

	uli.LowPart = fi.nFileSizeLow;
	uli.HighPart = fi.nFileSizeHigh;
	st->size = uli.QuadPart;

	st->type = LS_FT_FILE;
	if (fi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		st->type = LS_FT_DIR;
	else if (fi.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
		st->type = LS_FT_LINK;
	else if (fi.dwFileAttributes & FILE_ATTRIBUTE_DEVICE)
		st->type = LS_FT_DEV;

	uli.LowPart = fi.ftCreationTime.dwLowDateTime;
	uli.HighPart = fi.ftCreationTime.dwHighDateTime;
	st->ctime = uli.QuadPart;

	uli.LowPart = fi.ftLastAccessTime.dwLowDateTime;
	uli.HighPart = fi.ftLastAccessTime.dwHighDateTime;
	st->atime = uli.QuadPart;

	uli.LowPart = fi.ftLastWriteTime.dwLowDateTime;
	uli.HighPart = fi.ftLastWriteTime.dwHighDateTime;
	st->atime = uli.QuadPart;

	return 0;
#else
	struct stat pst;
	struct ls_file *pf;
	int flags;
	int rc;

	pf = ls_resolve_file(file, &flags);
	if (!pf)
		return -1;

	rc = fstat(pf->fd, &pst);
	if (rc == -1)
		return ls_set_errno(ls_errno_to_error(errno));

	st->size = pst.st_size;
	st->type = type_from_mode(pst.st_mode);

	return 0;
#endif // LS_WINDOWS
}

int ls_access(const char *path, int mode)
{
#if LS_WINDOWS
	WCHAR szPath[MAX_PATH];
	DWORD dwAccess;
	DWORD dwRet;

	if (ls_utf8_to_wchar_buf(path, szPath, MAX_PATH) == -1)
		return -1;

	dwAccess = ls_get_access_rights(mode);

	dwRet = GetFileAttributesW(szPath);
	if (dwRet == INVALID_FILE_ATTRIBUTES)
		return ls_set_errno_win32(GetLastError());

	if (dwAccess & LS_FILE_WRITE)
	{
		if (dwRet & FILE_ATTRIBUTE_READONLY)
			return ls_set_errno(LS_ACCESS_DENIED);
	}

	return 0;
#else
	int pmode;
	int rc;

	switch (mode) {
	case LS_FILE_READ:
		pmode = R_OK;
		break;
	case LS_FILE_WRITE:
		pmode = W_OK;
		break;
	case LS_FILE_EXECUTE:
		pmode = X_OK;
		break;
	case LS_FILE_EXIST:
		pmode = F_OK;
		break;
	default:
		return ls_set_errno(LS_INVALID_ARGUMENT);
	}

	rc = access(path, pmode);
	if (rc == -1)
		return ls_set_errno(ls_errno_to_error(errno));
	return 0;
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

static void ls_dir_data_dtor(struct ls_dir_data *data)
{
#if LS_WINDOWS
	if (data->hFind != INVALID_HANDLE_VALUE)
		FindClose(data->hFind);
#else
	if (data->unidir)
		closedir(data->unidir);
#endif // LS_WINDOWS
}

static const struct ls_class DirClass = {
	.type = LS_DIR,
	.cb = sizeof(struct ls_dir_data),
	.dtor = (ls_dtor_t)&ls_dir_data_dtor,
	.wait = NULL
};

ls_handle ls_opendir(const char *path)
{
#if LS_WINDOWS
	WCHAR szPath[MAX_PATH + 4]; // +4 for "/*\0"
	size_t len;
	struct ls_dir_data *data;

	len = ls_utf8_to_wchar_buf(path, szPath, MAX_PATH);
	if (len == -1)
		return NULL;

	if (szPath[len - 1] != L'\\')
		szPath[len++] = L'\\';
	szPath[len++] = L'*';
	szPath[len] = L'\0';

	data = ls_handle_create(&DirClass, 0);
	if (!data)
		return NULL;

	data->hFind = FindFirstFileW(szPath, &data->fd);
	if (data->hFind == INVALID_HANDLE_VALUE)
	{
		ls_set_errno_win32(GetLastError());
		ls_handle_dealloc(data);
		return NULL;
	}

	if (ls_wchar_to_utf8_buf(data->fd.cFileName, data->name, MAX_PATH) == -1)
	{
		FindClose(data->hFind);
		ls_handle_dealloc(data);
		return NULL;
	}

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

	if (!path)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	data = ls_handle_create(&DirClass, 0);
	if (!data)
		return NULL;

	data->unidir = opendir(path);
	if (!data->unidir)
	{
		ls_set_errno(ls_errno_to_error(errno));
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
	DWORD dwErr;

	if (!dir)
	{
		ls_set_errno(LS_INVALID_HANDLE);
		return NULL;
	}

	if (data->hFind == INVALID_HANDLE_VALUE)
	{
		ls_set_errno(LS_NO_MORE_FILES);
		return NULL;
	}

	if (ls_wchar_to_utf8_buf(data->fd.cFileName, data->name, MAX_PATH) == -1)
		return NULL;

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
		dwErr = GetLastError();
		if (dwErr = ERROR_NO_MORE_FILES)
		{
			FindClose(data->hFind);
			data->hFind = INVALID_HANDLE_VALUE;
		}
		else
		{
			ls_set_errno_win32(dwErr);
			return NULL;
		}
	}

	return &data->dir;
#else
	struct ls_dir_data *data = dir;
	struct dirent *dirent;
	struct stat st;
	int rc;

	if (!dir)
	{
		ls_set_errno(LS_INVALID_HANDLE);
		return NULL;
	}

	dirent = readdir(data->unidir);
	if (!dirent)
	{
		ls_set_errno(LS_NO_MORE_FILES);
		return NULL;
	}

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

struct ls_snapshot
{
	char *path;
	size_t path_len; // including null terminator
	char *name;
	size_t name_len; // including null terminator
	int flags;
	struct ls_stat st;
	struct ls_snapshot **subtree;
	size_t subtree_count;
};

static void ls_snapshot_dtor(struct ls_snapshot *ss)
{
	size_t i;

	for (i = 0; i < ss->subtree_count; i++)
		ls_close(ss->subtree[i]);

	ls_free(ss->path);
}

static const struct ls_class SnapshotClass = {
	.type = LS_SNAPSHOT,
	.cb = sizeof(struct ls_snapshot),
	.dtor = (ls_dtor_t)&ls_snapshot_dtor,
	.wait = NULL
};

ls_handle ls_snapshot_dir(const char *path, int flags, uint32_t max_depth)
{
	struct ls_snapshot *ss;
	int rc;
	int err;
	size_t len;

	if (!path)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	ss = ls_handle_create(&SnapshotClass, 0);
	if (!ss)
		return NULL;

	rc = ls_stat(path, &ss->st);
	if (rc == -1)
		goto failure;

	ss->path_len = ls_abspath(path, NULL, 0);
	if (ss->path_len == -1)
		goto failure;

	ss->path = ls_malloc(ss->path_len);
	if (!ss->path)
		goto failure;

	len = ls_abspath(path, ss->path, ss->path_len);
	if (len == -1)
		goto failure;

	ss->name = ls_strrdir(path);
	if (!ss->name)
		goto failure;
	ss->name++;
	ss->name_len = strlen(ss->name) + 1;

	// snapshot is valid here

	rc = ls_snapshot_refresh(ss, max_depth, NULL, NULL);
	if (rc == -1)
		goto failure;

	return ss;

failure:
	err = _ls_errno;
	ls_close(ss);
	_ls_errno = err;
	return NULL;
}

size_t ls_snapshot_path(ls_handle ssh, char *path, size_t size)
{
	struct ls_snapshot *ss;

	if (!ssh)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	if (!path != !size)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	ss = ssh;

	if (!path)
		return ss->path_len;

	if (ss->path_len > size)
		return ls_set_errno(LS_BUFFER_TOO_SMALL);

	memcpy(path, ss->path, ss->path_len);
	return ss->path_len - 1;
}

size_t ls_snapshot_name(ls_handle ssh, char *name, size_t size)
{
	struct ls_snapshot *ss;

	if (!ssh)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	if (!name != !size)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	ss = ssh;
	assert(strlen(ss->name) == ss->name_len - 1);

	if (!name)
		return ss->name_len;

	if (ss->name_len > size)
		return ls_set_errno(LS_BUFFER_TOO_SMALL);

	memcpy(name, ss->name, ss->name_len);
	return ss->name_len - 1;
}

int ls_snapshot_stat(ls_handle ssh, struct ls_stat *st)
{
	struct ls_snapshot *ss;

	if (!ssh || !st)
		return ls_set_errno(LS_INVALID_HANDLE);

	ss = ssh;
	memcpy(st, &ss->st, sizeof(struct ls_stat));
	return 0;
}

ls_handle ls_snapshot_enumerate(ls_handle ssh, void **it)
{
	struct ls_snapshot *ss;
	uintptr_t i;

	if (!ssh)
	{
		ls_set_errno(LS_INVALID_HANDLE);
		return NULL;
	}

	if (!it)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	ss = ssh;
	i = (uintptr_t)*it;

	if (ss->st.type != LS_FT_DIR)
	{
		ls_set_errno(LS_INVALID_STATE);
		return NULL;
	}

	if (i >= ss->subtree_count)
	{
		ls_set_errno(LS_NO_MORE_FILES);
		return NULL;
	}

	*it = (void *)(i + 1);

	return ss->subtree[i];
}

ls_handle ls_snapshot_lookup(ls_handle ssh, const char *path)
{
	struct ls_snapshot *ss;
	char *end;
	size_t len;
	size_t i;
	struct ls_snapshot *sub;

	if (!ssh)
	{
		ls_set_errno(LS_INVALID_HANDLE);
		return NULL;
	}

	if (!path)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	ss = ssh;

	if (!*path)
		return ss;

	if (ss->st.type != LS_FT_DIR)
	{
		ls_set_errno(LS_NOT_FOUND);
		return NULL;
	}

	end = ls_strdir(path);
	len = end - path;

	for (i = 0; i < ss->subtree_count; i++)
	{
		sub = ss->subtree[i];
		if (!strncmp(sub->name, path, len) && sub->name[len] == '\0')
			return ls_snapshot_lookup(sub, end + 1);
	}

	ls_set_errno(LS_NOT_FOUND);
	return NULL;
}

int ls_snapshot_refresh(ls_handle ssh, uint32_t max_depth, void(*cb)(const char *path, int event, void *up), void *up)
{
	struct ls_snapshot *ss;
	struct ls_stat st, st_old;
	int rc;
	size_t i, j;
	ls_handle dh;
	struct ls_dir *dir;
	size_t len;
	ls_handle hsub;
	size_t subpath_len;
	char *subpath;
	size_t rclen;
	struct ls_snapshot **tmp;
	struct ls_snapshot **sub;

	if (!ssh)
	{
		ls_set_errno(LS_INVALID_HANDLE);
		return -1;
	}

	ss = ssh;

	rc = ls_stat(ss->path, &st);
	if (rc == -1)
	{
		ls_set_errno(LS_NOT_FOUND);
		return -1;
	}

	memcpy(&st_old, &ss->st, sizeof(struct ls_stat));
	memcpy(&ss->st, &st, sizeof(struct ls_stat));

	if (st.type == LS_FT_DIR && max_depth != 0)
	{
		// remove deleted subdirectories
		for (i = 0; i < ss->subtree_count; i++)
		{
			rc = ls_snapshot_refresh(ss->subtree[i], max_depth - 1, cb, up);
			if (rc == -1)
			{
				if (cb)
					cb(ss->subtree[i]->path, LS_WATCH_REMOVE, up);

				ls_close(ss->subtree[i]);
				for (j = i; j < ss->subtree_count - 1; j++)
					ss->subtree[j] = ss->subtree[j + 1];
				ss->subtree_count--;
				i--;
				ss->subtree[i] = NULL;
			}
		}

		// add new subdirectories

		dh = ls_opendir(ss->path);
		if (!dh)
		{
			ls_set_errno(LS_NOT_FOUND);
			return -1;
		}

		len = strlen(ss->path);

		while ((dir = ls_readdir(dh)))
		{
			if (strcmp(dir->name, ".") == 0 || strcmp(dir->name, "..") == 0)
				continue;

			// check if subdirectory already exists
			hsub = ls_snapshot_lookup(ss, dir->name);
			if (hsub)
				continue;

			// new subdirectory, create snapshot

			subpath_len = len + 1 + strlen(dir->name) + 1;
			subpath = ls_malloc(subpath_len);
			if (!subpath)
				continue;

			rclen = ls_strcbcpy(subpath, ss->path, subpath_len);
			if (rclen == -1)
				goto subdir_continue;

			rclen = ls_strcbcat(subpath, LS_PATH_SEP_STR, subpath_len);
			if (rclen == -1)
				goto subdir_continue;

			rclen = ls_strcbcat(subpath, dir->name, subpath_len);
			if (rclen == -1)
				goto subdir_continue;

			if (cb)
				cb(subpath, LS_WATCH_ADD, up);

			tmp = ls_realloc(ss->subtree, (ss->subtree_count + 1) * sizeof(struct ls_subtree *));
			if (!tmp)
				goto subdir_continue;

			ss->subtree = tmp, tmp = NULL;

			sub = &ss->subtree[ss->subtree_count];

			// create snapshot of subtree
			*sub = ls_snapshot_dir(subpath, ss->flags, max_depth - 1);
			if (*sub)
				ss->subtree_count++;

		subdir_continue:
			ls_free(subpath), subpath = NULL;
		}

		ls_close(dh);
	}
	else
	{
		for (i = 0; i < ss->subtree_count; i++)
			ls_close(ss->subtree[i]);
		ls_free(ss->subtree), ss->subtree = NULL;
		ss->subtree_count = 0;
	}

	if (cb && memcmp(&st, &st_old, sizeof(struct ls_stat)))
		cb(ss->path, LS_WATCH_MODIFY, up);

	return 0;
}
