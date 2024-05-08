#include <lysys/ls_user.h>

#include <memory.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <lysys/ls_shell.h>
#include <lysys/ls_file.h>
#include <lysys/ls_stat.h>
#include <lysys/ls_core.h>

#include "ls_native.h"

size_t ls_username(char *name, size_t size)
{
#if LS_WINDOWS
	DWORD cbBuffer;
	WCHAR szBuffer[UNLEN + 1];
	BOOL b;
	char szName[UNLEN + 1];
	size_t rc;

	// name and size must be both set or unset
	if (!name != !size)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	cbBuffer = UNLEN + 1;
	b = GetUserNameW(szBuffer, &cbBuffer);
	if (!b)
		return ls_set_errno_win32(GetLastError());

	rc = ls_wchar_to_utf8_buf(szBuffer, szName, sizeof(szName));
	if (!rc)
		return -1;

	// return the required size
	if (size == 0)
		return rc;
	
	if (size < rc)
		return ls_set_errno(LS_BUFFER_TOO_SMALL);

	memcpy(name, szName, rc);
	return rc;
#else
	char *p;
	size_t len;

	if (!name != !size)
		return -1;

	p = getlogin();
	if (!p)
		return -1;

	len = strlen(p) + 1;
	if (size == 0)
		return len;

	if (len > size)
		len = size;

	memcpy(name, p, len);

	return len - 1;
#endif // LS_WINDOWS
}

size_t ls_home(char *path, size_t size)
{
#if LS_WINDOWS
	HANDLE hToken;
	WCHAR szBuffer[MAX_PATH];
	DWORD cbBuffer;
	BOOL b;
	char szPath[MAX_PATH];
	size_t rc;

	if (!path != !size)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	hToken = GetCurrentProcessToken();

	cbBuffer = MAX_PATH;
	b = GetUserProfileDirectoryW(hToken, szBuffer, &cbBuffer);
	if (!b)
		return ls_set_errno_win32(GetLastError());

	rc = ls_wchar_to_utf8_buf(szBuffer, szPath, sizeof(szPath));
	if (!rc)
		return -1;

	if (size == 0)
		return rc;

	if (size < rc)
		return ls_set_errno(LS_BUFFER_TOO_SMALL);

	memcpy(path, szPath, rc);
	return rc;
#else
	return ls_getenv_buf("HOME", path, size);
#endif // LS_WINDOWS
}

#if LS_POSIX

// search for a directory in the user-dirs.dirs file
static size_t ls_xdg_user_dir(const char *name, char *buf, size_t buflen)
{
	size_t len;
	ls_handle fh;
	struct ls_stat st;
	int rc;
	char *userdirs;
	size_t size;
	char *line;
	char *token;

	char key[48];
	int keylen;

	char *value;
	size_t vallen;

	// read the user-dirs.dirs file
	len = ls_expand_env("$HOME/.config/user-dirs.dirs", buf, buflen);
	if (len == -1)
		return -1;

	rc = ls_stat(buf, &st);
	if (rc == -1)
		return -1;

	userdirs = ls_malloc(st.size + 2);
	if (!userdirs)
		return -1;

	fh = ls_open(buf, LS_FILE_READ, LS_SHARE_READ, LS_OPEN_EXISTING);
	if (fh == NULL)
	{
		ls_free(userdirs);
		return -1;
	}
	
	size = ls_read(fh, userdirs, st.size);
	if (size == -1)
	{
		ls_close(fh);
		ls_free(userdirs);
		return -1;
	}

	ls_close(fh);

	// add a newline at the end of the file
	userdirs[size] = '\n';
	userdirs[size + 1] = '\0';
	
	// create the key
	keylen = snprintf(key, sizeof(key), "XDG_%s_DIR=", name);
	if (keylen < 0)
	{
		ls_free(userdirs);
		return -1;
	}

	// search for the key
	token = userdirs;
	while ((line = strsep(&token, "\n")))
	{
		if (!line[0] || line[0] != '#') // comment
			continue;
	
		// check if the key is at the beginning of the line
		if (!strncmp(line, key, keylen))
		{
			value = line + keylen;
			vallen = strlen(value);

			// expand the value
			len = ls_expand_env(value, buf, buflen);
			ls_free(userdirs);
			return len;
		}
	}

	ls_free(userdirs);
	
	return -1;
}

#endif // LS_POSIX

size_t ls_common_dir(int dir, char *path, size_t size)
{
#if LS_WINDOWS
	HANDLE hToken;
	HRESULT hr;
	LPWSTR szPath;
	const KNOWNFOLDERID *rfid;
	char szResult[MAX_PATH];
	size_t rc;

	if (!path != !size)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	switch (dir)
	{
	default:
		return ls_set_errno(LS_NOT_FOUND);
	case LS_DIR_USER_HOME:
		rfid = &FOLDERID_Profile;
		break;
	case LS_DIR_USER_DOCUMENTS:
		rfid = &FOLDERID_Documents;
		break;
	case LS_DIR_USER_PICTURES:
		rfid = &FOLDERID_Pictures;
		break;
	case LS_DIR_USER_MUSIC:
		rfid = &FOLDERID_Music;
		break;
	case LS_DIR_USER_VIDEOS:
		rfid = &FOLDERID_Videos;
		break;
	case LS_DIR_USER_DOWNLOADS:
		rfid = &FOLDERID_Downloads;
		break;
	case LS_DIR_USER_DESKTOP:
		rfid = &FOLDERID_Desktop;
		break;
	case LS_DIR_USER_TEMPLATES:
		rfid = &FOLDERID_Templates;
		break;
	case LS_DIR_USER_PUBLIC:
		rfid = &FOLDERID_Public;
		break;
	case LS_DIR_WINDOWS:
		rfid = &FOLDERID_Windows;
		break;
	case LS_DIR_SYSTEM32:
		rfid = &FOLDERID_System;
		break;
	case LS_DIR_PROGRAM_FILES:
		rfid = &FOLDERID_ProgramFiles;
		break;
	case LS_DIR_PROGRAM_FILES_X86:
		rfid = &FOLDERID_ProgramFilesX86;
		break;
	}

	hToken = GetCurrentProcessToken();

	hr = SHGetKnownFolderPath(rfid, 0, hToken, &szPath);
	if (!SUCCEEDED(hr))
		return ls_set_errno_hresult(hr);

	rc = ls_wchar_to_utf8_buf(szPath, szResult, sizeof(szResult));

	CoTaskMemFree(szPath);

	if (!rc)
		return -1;

	if (size == 0)
		return rc;

	if (size < rc)
		return ls_set_errno(LS_BUFFER_TOO_SMALL);

	memcpy(path, szResult, rc);
	return rc;
#else
	char buf[PATH_MAX];
	size_t len;

	if (!path != !size)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	switch (dir)
	{
	default:
		return ls_set_errno(LS_NOT_FOUND);
	case LS_DIR_USER_HOME:
		return ls_home(path, size);
	case LS_DIR_USER_DOCUMENTS:
		len = ls_xdg_user_dir("DOCUMENTS", buf, sizeof(buf));
		break;
	case LS_DIR_USER_PICTURES:
		len = ls_xdg_user_dir("PICTURES", buf, sizeof(buf));
		break;
	case LS_DIR_USER_MUSIC:
		len = ls_xdg_user_dir("MUSIC", buf, sizeof(buf));
		break;
	case LS_DIR_USER_VIDEOS:
		len = ls_xdg_user_dir("VIDEOS", buf, sizeof(buf));
		break;
	case LS_DIR_USER_DOWNLOADS:
		len = ls_xdg_user_dir("DOWNLOAD", buf, sizeof(buf));
		break;
	case LS_DIR_USER_DESKTOP:
		len = ls_xdg_user_dir("DESKTOP", buf, sizeof(buf));
		break;
	case LS_DIR_USER_TEMPLATES:
		len = ls_xdg_user_dir("TEMPLATES", buf, sizeof(buf));
		break;
	case LS_DIR_USER_PUBLIC:
		len = ls_xdg_user_dir("PUBLICSHARE", buf, sizeof(buf));
		break;
	};

	if (len == -1)
		return -1;

	len++;
	if (path == NULL && size == 0)
		return len;

	if (size < len)
		return -1;

	memcpy(path, buf, len);
	return len - 1;
#endif // LS_WINDOWS 
}
