#include "ls_native.h"

#include <lysys/ls_shell.h>

#include <lysys/ls_core.h>

#include <string.h>
#include <stdlib.h>

char *ls_strdir(const char *path)
{
	while (*path && *path != '/' && *path != '\\')
		path++;
	return (char *)path;
}

char *ls_strrdir(const char *path)
{
	char *last = NULL;
	while (*path)
	{
		if (*path == '/' || *path == '\\')
			last = (char *)path;
		path++;
	}
	return last;
}

size_t ls_dirname(const char *path, char *buf, size_t size)
{
	char *sep;

	sep = ls_strrdir(path);
	if (!sep) return 0;

	return ls_substr(path, sep - path, buf, size);
}

size_t ls_basename(const char *path, char *buf, size_t size)
{
	char *sep;
	size_t len;

	sep = ls_strrdir(path);
	if (!sep) return 0;

	len = strlen(++sep);
	return ls_substr(sep, len, buf, size);
}

size_t ls_getenv_buf(const char *name, char *buf, size_t size)
{
#if LS_WINDOWS
	LPWSTR lpName;
	DWORD dwLen;
	LPWSTR lpValue;
	size_t r;

	lpName = ls_utf8_to_wchar(name);
	if (!lpName) return 0;

	dwLen = GetEnvironmentVariableW(lpName, NULL, 0);
	if (buf == NULL || dwLen == 0)
	{
		ls_free(lpName);
		return 0;
	}

	lpValue = ls_malloc(dwLen * sizeof(WCHAR));
	if (!GetEnvironmentVariableW(lpName, lpValue, dwLen))
	{
		ls_free(lpName);
		ls_free(lpValue);
		return 0;
	}

	r = ls_wchar_to_utf8_buf(lpValue, buf, (int)size);

	ls_free(lpValue);
	ls_free(lpName);

	return r - 1; // Exclude the null terminator
#else
    const char *env;
    size_t len;
    
    if (size == 0)
        return 0;
    
    env = getenv(name);
    if (!env)
        return NULL;
    
    len = strlen(env);
    if (len > size)
        len = size - 1;
    
    memcpy(buf, env, len);
    buf[len] = 0;
    
	return len;
#endif // LS_WINDOWS
}

char *ls_getenv(const char *name)
{
#if LS_WINDOWS
	LPWSTR lpName;
	DWORD dwLen;
	LPWSTR lpValue;
	char *ret;

	lpName = ls_utf8_to_wchar(name);
	if (!lpName) return 0;

	dwLen = GetEnvironmentVariableW(lpName, NULL, 0);
	if (dwLen == 0)
	{
		ls_free(lpName);
		return 0;
	}

	lpValue = ls_malloc(dwLen * sizeof(WCHAR));
	if (!GetEnvironmentVariableW(lpName, lpValue, dwLen))
	{
		ls_free(lpName);
		ls_free(lpValue);
		return 0;
	}

	ret = ls_wchar_to_utf8(lpValue);

	ls_free(lpValue);
	ls_free(lpName);

	return ret;
#else
    const char *env;
    
    env = getenv(name);
    if (!env)
        return NULL;
    
    return ls_strdup(env);
#endif // LS_WINDOWS
}

size_t ls_expand_env(const char *src, char *dst, size_t size)
{
#if LS_WINDOWS
	LPWSTR lpSrc;
	DWORD dwLen;
	LPWSTR lpDst;
	size_t r;

	lpSrc = ls_utf8_to_wchar(src);
	if (!lpSrc) return 0;

	dwLen = ExpandEnvironmentStringsW(lpSrc, NULL, 0);
	if (dst == NULL || dwLen == 0)
	{
		ls_free(lpSrc);
		return 0;
	}

	lpDst = ls_malloc(dwLen * sizeof(WCHAR));
	if (!ExpandEnvironmentStringsW(lpSrc, lpDst, dwLen))
	{
		ls_free(lpSrc);
		ls_free(lpDst);
		return 0;
	}

	r = ls_wchar_to_utf8_buf(lpDst, dst, (int)size);

	ls_free(lpDst);
	ls_free(lpSrc);

	return r - 1; // Exclude the null terminator
#else
	return 0;
#endif // LS_WINDOWS
}

size_t ls_which(const char *path, char *buf, size_t size)
{
#if LS_WINDOWS
	LPWSTR lpPath;
	DWORD dwLen;
	LPWSTR lpBuf;
	size_t r;

	lpPath = ls_utf8_to_wchar(path);
	if (!lpPath) return 0;

	dwLen = SearchPathW(NULL, lpPath, NULL, 0, NULL, NULL);
	if (buf == NULL || dwLen == 0)
	{
		ls_free(lpPath);
		return 0;
	}

	lpBuf = ls_malloc(dwLen * sizeof(WCHAR));
	if (!SearchPathW(NULL, lpPath, NULL, dwLen, lpBuf, NULL))
	{
		ls_free(lpPath);
		ls_free(lpBuf);
		return 0;
	}

	r = ls_wchar_to_utf8_buf(lpBuf, buf, (int)size);

	ls_free(lpBuf);
	ls_free(lpPath);

	return r - 1; // Exclude the null terminator
#else
    char *syspath, *string;
    char *token;
    int rc;
    char fullpath[PATH_MAX];
    size_t len = 0;
    
    if (size == 0)
        return 0;
    
    syspath = ls_getenv("HOME");
    if (!syspath)
        return 0;
    
    string = syspath;
    while ((token = strsep(&string, ":")) != NULL)
    {
        strncpy(fullpath, token, PATH_MAX);
        strncat(fullpath, path, PATH_MAX);
        
        rc = access(fullpath, F_OK);
        if (rc == 0)
        {
            len = strlen(fullpath);
            if (len > size)
                len = size - 1;
            memcpy(buf, fullpath, len);
            buf[len] = 0;
            break;
        }
    }
    
    ls_free(syspath);
    
	return len;
#endif // LS_WINDOWS
}

size_t ls_abspath(const char *path, char *buf, size_t size)
{
#if LS_WINDOWS
	WCHAR szPath[MAX_PATH];
	WCHAR szBuf[MAX_PATH];
	SIZE_T stLen;

	ls_utf8_to_wchar_buf(path, szPath, sizeof(szPath));
	stLen = GetFullPathNameW(szPath, 0, szBuf, NULL);
	ls_wchar_to_utf8_buf(szBuf, buf, (int)size);

	return stLen - 1; // Exclude the null terminator
#else
    size_t len;
    char *wd;
    
    if (size == 0)
        return 0;
    
    len = strlen(path);
    if (len == 0)
        return 0;
    
    if (path[0] == '/')
    {
        if (len > size)
            len = size - 1;
        memcpy(buf, path, len);
        buf[len] = 0;
        return len;
    }
    
    wd = getcwd(buf, size);
    if (!wd)
        return 0;
    
    len = strlcat(buf, path, size);
    if (len > size)
        return size;
    
    return len;
#endif // LS_WINDOWS
}

size_t ls_relpath(const char *path, const char *base, char *buf, size_t size)
{
#if LS_WINDOWS
	WCHAR szPath[MAX_PATH];
	WCHAR szBase[MAX_PATH];
	WCHAR szBuf[MAX_PATH];
	BOOL r;

	ls_utf8_to_wchar_buf(path, szPath, sizeof(szPath));
	ls_utf8_to_wchar_buf(base, szBase, sizeof(szBase));

	r = PathRelativePathToW(szBuf, szPath, FILE_ATTRIBUTE_DIRECTORY, szBase, FILE_ATTRIBUTE_DIRECTORY);

	if (!r) return 0;

	return ls_wchar_to_utf8_buf(szBuf, buf, (int)size) - 1ULL; // Exclude the null terminator
#else
	return 0;
#endif // LS_WINDOWS
}

size_t ls_realpath(const char *path, char *buf, size_t size)
{
#if LS_WINDOWS
	HANDLE hFile;
	WCHAR szPath[MAX_PATH];
	DWORD r;

	ls_utf8_to_wchar_buf(path, szPath, sizeof(szPath));
	hFile = CreateFileW(szPath, 0, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) return 0;

	r = GetFinalPathNameByHandleW(hFile, szPath, MAX_PATH, 0);
	CloseHandle(hFile);

	if (!r) return 0;

	return ls_wchar_to_utf8_buf(szPath, buf, (int)size) - 1; // Exclude the null terminator
#else
	return 0;
#endif // LS_WINDOWS
}

size_t ls_cwd(char *buf, size_t size)
{
#if LS_WINDOWS
	DWORD r;
	WCHAR szBuf[MAX_PATH];

	if (size == 0)
		return 0;

	if (!buf)
	{
		r = GetCurrentDirectoryW(0, NULL);
		if (r == 0)
			return 0;

		return r - 1; // Exclude the null terminator
	}

	r = GetCurrentDirectoryW(MAX_PATH, szBuf);
	if (r == 0)
		return 0;

	return ls_wchar_to_utf8_buf(szBuf, buf, (int)size) - 1; // Exclude the null terminator
#else
	return 0;
#endif // LS_WINDOWS
}
