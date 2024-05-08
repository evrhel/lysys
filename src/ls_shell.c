#include "ls_native.h"

#include <lysys/ls_shell.h>
#include <lysys/ls_file.h>
#include <lysys/ls_core.h>

#include <string.h>
#include <stdlib.h>

char *ls_strdir(const char *path)
{
	if (!path)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	while (*path && *path != '/' && *path != '\\')
		path++;
	return (char *)path;
}

char *ls_strrdir(const char *path)
{
	const char *last = NULL;

	if (!path)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	while (*path)
	{
		if (*path == '/' || *path == '\\')
			last = path;
		path++;
	}
	return (char *)last;
}

void ls_path_win32(char *path)
{
	while (*path)
	{
		if (*path == '/')
			*path = '\\';
		path++;
	}
}

void ls_path_unix(char *path)
{
	while (*path)
	{
		if (*path == '\\')
			*path = '/';
		path++;
	}
}

void ls_path_native(char *path)
{
#if LS_WINDOWS
	ls_path_win32(path);
#else
	ls_path_unix(path);
#endif // LS_WINDOWS
}

size_t ls_dirname(const char *path, char *buf, size_t size)
{
	char *sep;
	size_t len;

	if (!path || !buf != !size)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	sep = ls_strrdir(path);
	if (!sep)
		return 0;

	len = sep - path + 1;
	if (size == 0)
		return len;

	if (size < len)
		return ls_set_errno(LS_BUFFER_TOO_SMALL);

	return ls_substr(path, len-1, buf);
}

size_t ls_basename(const char *path, char *buf, size_t size)
{
	char *sep;
	size_t len;

	if (!path || !buf != !size)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	sep = ls_strrdir(path);
	if (!sep)
		return 0;

	len = strlen(++sep) + 1;

	if (size == 0)
		return len;

	if (size < len)
	{
		ls_set_errno(LS_BUFFER_TOO_SMALL);
		return -1;
	}

	return ls_substr(sep, len-1, buf);
}

size_t ls_getenv_buf(const char *name, char *buf, size_t size)
{
#if LS_WINDOWS
	LPWSTR lpName;
	DWORD dwLen;
	LPWSTR lpValue;
	size_t r;

	if (!name || !buf != !size)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	lpName = ls_utf8_to_wchar(name);
	if (!lpName)
		return -1;

	dwLen = GetEnvironmentVariableW(lpName, NULL, 0);
	if (dwLen == 0)
	{
		ls_set_errno_win32(GetLastError());
		ls_free(lpName);
		return -1;
	}

	if (size == 0)
	{
		ls_free(lpName);
		return dwLen;
	}

	lpValue = ls_malloc(dwLen * sizeof(WCHAR));
	if (!lpValue)
	{
		ls_free(lpName);
		return -1;
	}

	if (!GetEnvironmentVariableW(lpName, lpValue, dwLen))
	{
		ls_set_errno_win32(GetLastError());
		ls_free(lpValue);
		ls_free(lpName);
		return -1;
	}

	r = ls_wchar_to_utf8_buf(lpValue, buf, size);

	ls_free(lpValue);
	ls_free(lpName);

	return r;
#else
    const char *env;
    size_t len;
        
    env = getenv(name);
    if (!env)
	{
		ls_set_errno(LS_NOT_FOUND);
        return -1;
	}
    
    len = strlen(env) + 1;
	if (size == 0)
		return len;

	if (len > size)
	{
		ls_set_errno(LS_BUFFER_TOO_SMALL);
		return -1;
	}
    
    memcpy(buf, env, len);

	return len - 1;
#endif // LS_WINDOWS
}

char *ls_getenv(const char *name)
{
	char *buf;
	size_t len;

	len = ls_getenv_buf(name, NULL, 0);
	if (len == -1)
		return NULL;

	buf = ls_malloc(len);
	if (!buf)
		return NULL;

	len = ls_getenv_buf(name, buf, len);
	if (len == -1)
	{
		ls_free(buf);
		return NULL;
	}

	return buf;
}

size_t ls_expand_env(const char *src, char *dst, size_t size)
{
#if LS_WINDOWS
	LPWSTR lpSrc;
	DWORD dwLen;
	LPWSTR lpDst;
	size_t r;

	if (!src || !dst != !size)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	lpSrc = ls_utf8_to_wchar(src);
	if (!lpSrc)
		return -1;

	dwLen = ExpandEnvironmentStringsW(lpSrc, NULL, 0);
	if (dwLen == 0)
	{
		ls_set_errno_win32(GetLastError());
		ls_free(lpSrc);
		return -1;
	}

	lpDst = ls_malloc(dwLen * sizeof(WCHAR));
	if (!lpDst)
	{
		ls_free(lpSrc);
		return -1;
	}

	if (!ExpandEnvironmentStringsW(lpSrc, lpDst, dwLen))
	{
		ls_set_errno_win32(GetLastError());
		ls_free(lpSrc);
		ls_free(lpDst);
		return -1;
	}

	r = ls_wchar_to_utf8_buf(lpDst, dst, size);

	ls_free(lpDst);
	ls_free(lpSrc);

	return r;
#else
	wordexp_t p;
	char **w;
	int rc;

	char *result, *newp;
	size_t result_size;
	size_t len;
	size_t new_size;

	rc = wordexp(src, &p, 0);
	if (rc != 0)
	{
		if (rc == WRDE_NOSPACE)
			return ls_set_errno(LS_OUT_OF_MEMORY);
		return ls_set_errno(LS_UNKNOWN_ERROR);
	}

	result_size = 0;
	result = NULL;

	for (w = p.we_wordv; *w; w++)
	{
		len = strlen(*w);
		new_size = result_size + len + 1;
		newp = ls_realloc(result, new_size);
		if (newp == NULL)
		{
			ls_free(result);
			wordfree(&p);
			return -1;
		}

		result = newp;

		memcpy(result + result_size, *w, len);
		result[result_size + len] = ' ';

		result_size = new_size;
	}

	wordfree(&p);

	if (size == 0)
	{
		ls_free(result);
		return result_size;
	}

	if (size < result_size + 1)
	{
		ls_set_errno(LS_BUFFER_TOO_SMALL);
		ls_free(result);
		return -1;
	}

	result_size--;
	memcpy(dst, result, result_size);
	dst[result_size] = '\0';

	ls_free(result);
	return result_size;
#endif // LS_WINDOWS
}

size_t ls_which(const char *path, char *buf, size_t size)
{
#if LS_WINDOWS
	LPWSTR lpPath;
	DWORD dwLen;
	LPWSTR lpBuf;
	size_t r;

	if (!path || !buf != !size)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	lpPath = ls_utf8_to_wchar(path);
	if (!lpPath)
		return -1;

	dwLen = SearchPathW(NULL, lpPath, NULL, 0, NULL, NULL);
	if (dwLen == 0)
	{
		ls_set_errno_win32(GetLastError());
		ls_free(lpPath);
		return -1;
	}

	lpBuf = ls_malloc(dwLen * sizeof(WCHAR));
	if (!lpBuf)
	{
		ls_free(lpPath);
		return -1;
	}

	if (!SearchPathW(NULL, lpPath, NULL, dwLen, lpBuf, NULL))
	{
		ls_set_errno_win32(GetLastError());
		ls_free(lpPath);
		ls_free(lpBuf);
		return -1;
	}

	r = ls_wchar_to_utf8_buf(lpBuf, buf, size);

	ls_free(lpBuf);
	ls_free(lpPath);

	return r;
#else
    char *syspath, *string;
    char *token;
    int rc;
    char fullpath[PATH_MAX];
    size_t len = 0;
        
    syspath = ls_getenv("HOME");
    if (!syspath)
        return -1;
    
    string = syspath;
    while ((token = strsep(&string, ":")) != NULL)
    {
        strncpy(fullpath, token, PATH_MAX);
        strncat(fullpath, path, PATH_MAX);
        
        rc = access(fullpath, F_OK);
        if (rc == 0)
        {
            len = strlen(fullpath) + 1;
			if (size == 0)
			{
				ls_free(syspath);
				return len;
			}

            if (len > size)
			{
				ls_set_errno(LS_BUFFER_TOO_SMALL);
				ls_free(syspath);
				return -1;
			}

            memcpy(buf, fullpath, len);

			ls_free(syspath);

			return len - 1;
        }
    }
    
	ls_free(syspath);
    
	ls_set_errno(LS_FILE_NOT_FOUND);
	return -1;
#endif // LS_WINDOWS
}

size_t ls_abspath(const char *path, char *buf, size_t size)
{
#if LS_WINDOWS
	WCHAR szPath[MAX_PATH];
	WCHAR szBuf[MAX_PATH];
	SIZE_T stLen;
	char szResult[MAX_PATH];
	size_t rc;

	if (!path || !buf != !size)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	if (ls_utf8_to_wchar_buf(path, szPath, sizeof(szPath)) == -1)
		return -1;

	stLen = GetFullPathNameW(szPath, MAX_PATH, szBuf, NULL);
	if (!stLen)
		return ls_set_errno_win32(GetLastError());

	rc = ls_wchar_to_utf8_buf(szBuf, szResult, MAX_PATH);
	if (rc == -1)
		return -1;
	
	rc++;
	if (size == 0)
		return rc;

	if (size < rc)
		return ls_set_errno(LS_BUFFER_TOO_SMALL);

	memcpy(buf, szResult, rc);

	return rc - 1;
#else
    size_t len, wdlen;
	size_t full_size;
        
	if (size == 0 && buf != NULL)
		return ls_set_errno(LS_INVALID_ARGUMENT);
	if (buf == NULL && size != 0)
		return ls_set_errno(LS_INVALID_ARGUMENT);

    len = strlen(path) + 1;
	if (len == 1)
		return 0;

	if (size == 0)
		return len;
    
    if (path[0] == '/')
    {
        if (full_size > size)
            return ls_set_errno(LS_BUFFER_TOO_SMALL);

        memcpy(buf, path, len);
        return len;
    }
    
    wdlen = ls_cwd(buf, size);
    if (wdlen == -1)
        return ls_set_errno(ls_errno_to_error(errno));
		
	full_size = wdlen + 1 + len; // wd + '/' + path
	if (size == 0)
		return full_size;
    
	if (full_size > size)
		return ls_set_errno(LS_BUFFER_TOO_SMALL);

	buf[wdlen] = '/';
	memcpy(buf + wdlen + 1, path, len);

    return full_size - 1;
#endif // LS_WINDOWS
}

size_t ls_relpath(const char *path, const char *base, char *buf, size_t size)
{
#if LS_WINDOWS
	WCHAR szPath[MAX_PATH];
	WCHAR szBase[MAX_PATH];
	WCHAR szBuf[MAX_PATH];
	BOOL r;
	char szResult[MAX_PATH];
	size_t rc;

	if (!path || !base || !buf != !size)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	if (ls_utf8_to_wchar_buf(path, szPath, sizeof(szPath)) == -1)
		return -1;

	if (ls_utf8_to_wchar_buf(base, szBase, sizeof(szBase)) == -1)
		return -1;

	r = PathRelativePathToW(szBuf, szPath, FILE_ATTRIBUTE_DIRECTORY, szBase, FILE_ATTRIBUTE_DIRECTORY);
	if (!r)
		return ls_set_errno_win32(GetLastError());

	rc = ls_wchar_to_utf8_buf(szBuf, szResult, MAX_PATH);
	if (rc == -1)
		return -1;

	if (size == 0)
		return rc;

	if (size < rc)
		return ls_set_errno(LS_BUFFER_TOO_SMALL);
	
	memcpy(buf, szResult, rc);
	return rc;
#else
	ls_set_errno(LS_NOT_IMPLEMENTED);
	return -1;
#endif // LS_WINDOWS
}

size_t ls_realpath(const char *path, char *buf, size_t size)
{
#if LS_WINDOWS
	HANDLE hFile;
	WCHAR szPath[MAX_PATH];
	DWORD r;
	char szResult[MAX_PATH];
	size_t rc;

	if (!path || !buf != !size)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	if (ls_utf8_to_wchar_buf(path, szPath, sizeof(szPath)) == -1)
		return -1;

	hFile = CreateFileW(szPath, 0, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return ls_set_errno_win32(GetLastError());

	r = GetFinalPathNameByHandleW(hFile, szPath, MAX_PATH, 0);
	CloseHandle(hFile);

	if (!r)
		return ls_set_errno(LS_UNKNOWN_ERROR);

	rc = ls_wchar_to_utf8_buf(szPath, szResult, MAX_PATH);
	if (rc == -1)
		return -1;

	if (size == 0)
		return rc;

	if (size < rc)
		return ls_set_errno(LS_BUFFER_TOO_SMALL);

	memcpy(buf, szResult, rc);
	return rc;
#else
	ls_set_errno(LS_NOT_IMPLEMENTED);
	return -1;
#endif // LS_WINDOWS
}

size_t ls_cwd(char *buf, size_t size)
{
#if LS_WINDOWS
	DWORD r;
	WCHAR szBuf[MAX_PATH];
	char szResult[MAX_PATH];
	size_t rc;

	if (!buf != !size)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	r = GetCurrentDirectoryW(MAX_PATH, szBuf);
	if (r == 0)
		return ls_set_errno(LS_UNKNOWN_ERROR);

	rc = ls_wchar_to_utf8_buf(szBuf, szResult, MAX_PATH);
	if (rc == -1)
		return -1;

	if (size == 0)
		return rc;

	if (size < rc)
		return ls_set_errno(LS_BUFFER_TOO_SMALL);

	memcpy(buf, szResult, rc);
	return rc;
#else
	size_t len;
	char tbuf[PATH_MAX];
	char *r;

	if (size == 0 && buf != NULL)
		return ls_set_errno(LS_INVALID_ARGUMENT);
	if (buf == NULL && size != 0)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	r = getcwd(tbuf, PATH_MAX);
	if (r == NULL)
		return ls_set_errno(ls_errno_to_error(errno));

	len = strlen(tbuf) + 1;
	if (buf == NULL && size == 0)
		return len;

	if (size < len)
		return ls_set_errno(LS_BUFFER_TOO_SMALL);

	memcpy(buf, r, len);
	return len - 1;
#endif // LS_WINDOWS
}

#if LS_WINDOWS
static int ls_parse_move_names(const char *src, const char *dst, PWCHAR wszSrc, PWCHAR wszDstName, PWCHAR wszDstDir)
{
	size_t len;
	PWSTR pszMatch;

	len = ls_utf8_to_wchar_buf(src, wszSrc, MAX_PATH);
	if (len == -1)
		return -1;

	len = ls_utf8_to_wchar_buf(dst, wszDstDir, MAX_PATH);
	if (len == -1)
		return -1;

	pszMatch = (PWSTR)StrRChrW(wszDstDir, NULL, L'\\');
	if (!pszMatch)
		return ls_set_errno(LS_INVALID_ARGUMENT);
	*pszMatch = L'\0';

	wcscpy_s(wszDstName, MAX_PATH, pszMatch + 1);

	return 0;
}

static IShellItem *ls_item_from_parsing_name(PCWSTR pwzPath, IBindCtx *ctx)
{
	IShellItem *psi = NULL;
	HRESULT hr;

	hr = SHCreateItemFromParsingName(pwzPath, ctx, &IID_IShellItem, (void **)&psi);
	if (FAILED(hr))
	{
		ls_set_errno_hresult(hr);
		return NULL;
	}

	return psi;
}
#endif // LS_WINDOWS

int ls_shell_move(const char *src, const char *dst)
{
#if LS_WINDOWS
	WCHAR wszSrc[MAX_PATH];
	WCHAR wszDstName[MAX_PATH];
	WCHAR wszDstDir[MAX_PATH];
	int rc;
	HRESULT hr;
	IFileOperation *pfo = NULL;
	IShellItem *psiFrom;
	IShellItem *psiTo;

	rc = ls_parse_move_names(src, dst, wszSrc, wszDstName, wszDstDir);
	if (rc == -1)
		return -1;

	hr = CoCreateInstance(&CLSID_FileOperation, NULL, CLSCTX_ALL, &IID_IFileOperation, (void **)&pfo);
	if (FAILED(hr))
		return ls_set_errno_hresult(hr);

	hr = pfo->lpVtbl->SetOperationFlags(pfo, FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT | FOF_ALLOWUNDO);
	if (FAILED(hr))
	{
		pfo->lpVtbl->Release(pfo);
		return ls_set_errno_hresult(hr);
	}

	psiFrom = ls_item_from_parsing_name(wszSrc, NULL);
	if (!psiFrom)
	{
		pfo->lpVtbl->Release(pfo);
		return -1;
	}

	psiTo = ls_item_from_parsing_name(wszDstDir, NULL);
	if (!psiTo)
	{
		psiFrom->lpVtbl->Release(psiFrom);
		pfo->lpVtbl->Release(pfo);
		return -1;
	}

	hr = pfo->lpVtbl->MoveItem(pfo, psiFrom, psiTo, wszDstName, NULL);
	if (FAILED(hr))
	{
		psiTo->lpVtbl->Release(psiTo);
		psiFrom->lpVtbl->Release(psiFrom);
		pfo->lpVtbl->Release(pfo);
		return ls_set_errno_hresult(hr);
	}

	hr = pfo->lpVtbl->PerformOperations(pfo);
	
	psiTo->lpVtbl->Release(psiTo);
	psiFrom->lpVtbl->Release(psiFrom);
	pfo->lpVtbl->Release(pfo);

	return ls_set_errno_hresult(hr);
#else
	return ls_move(src, dst);
#endif // LS_WINDOWS
}

int ls_shell_copy(const char *src, const char *dst)
{
#if LS_WINDOWS
	WCHAR wszSrc[MAX_PATH];
	WCHAR wszDst[MAX_PATH];
	size_t len;
	HRESULT hr;
	IFileOperation *pfo = NULL;
	IShellItem *psiFrom;
	IShellItem *psiTo;

	len = ls_utf8_to_wchar_buf(src, wszSrc, MAX_PATH);
	if (len == -1)
		return -1;

	len = ls_utf8_to_wchar_buf(dst, wszDst, MAX_PATH);
	if (len == -1)
		return -1;

	hr = CoCreateInstance(&CLSID_FileOperation, NULL, CLSCTX_ALL, &IID_IFileOperation, (void **)&pfo);
	if (FAILED(hr))
		return ls_set_errno_hresult(hr);

	hr = pfo->lpVtbl->SetOperationFlags(pfo, FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT | FOF_ALLOWUNDO);
	if (FAILED(hr))
	{
		pfo->lpVtbl->Release(pfo);
		return ls_set_errno_hresult(hr);
	}

	psiFrom = ls_item_from_parsing_name(wszSrc, NULL);
	if (!psiFrom)
	{
		pfo->lpVtbl->Release(pfo);
		return -1;
	}

	psiTo = ls_item_from_parsing_name(wszDst, NULL);
	if (!psiTo)
	{
		psiFrom->lpVtbl->Release(psiFrom);
		pfo->lpVtbl->Release(pfo);
		return -1;
	}

	hr = pfo->lpVtbl->CopyItem(pfo, psiFrom, psiTo, NULL, NULL);
	if (FAILED(hr))
	{
		psiTo->lpVtbl->Release(psiTo);
		psiFrom->lpVtbl->Release(psiFrom);
		pfo->lpVtbl->Release(pfo);
		return ls_set_errno_hresult(hr);
	}

	hr = pfo->lpVtbl->PerformOperations(pfo);

	psiTo->lpVtbl->Release(psiTo);
	psiFrom->lpVtbl->Release(psiFrom);
	pfo->lpVtbl->Release(pfo);

	return ls_set_errno_hresult(hr);
#else
	return ls_copy(src, dst);
#endif // LS_WINDOWS
}

int ls_shell_delete(const char *path)
{
#if LS_WINDOWS
	WCHAR szPath[MAX_PATH];
	size_t len;
	HRESULT hr;
	IFileOperation *pfo = NULL;
	IShellItem *psi;

	len = ls_utf8_to_wchar_buf(path, szPath, MAX_PATH);
	if (len == -1)
		return -1;

	hr = CoCreateInstance(&CLSID_FileOperation, NULL, CLSCTX_ALL, &IID_IFileOperation, (void **)&pfo);
	if (FAILED(hr))
		return ls_set_errno_hresult(hr);

	hr = pfo->lpVtbl->SetOperationFlags(pfo, FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT);
	if (FAILED(hr))
	{
		pfo->lpVtbl->Release(pfo);
		return ls_set_errno_hresult(hr);
	}

	psi = ls_item_from_parsing_name(szPath, NULL);
	if (!psi)
	{
		pfo->lpVtbl->Release(pfo);
		return -1;
	}

	hr = pfo->lpVtbl->DeleteItem(pfo, psi, NULL);
	if (FAILED(hr))
	{
		psi->lpVtbl->Release(psi);
		pfo->lpVtbl->Release(pfo);
		return ls_set_errno_hresult(hr);
	}

	hr = pfo->lpVtbl->PerformOperations(pfo);

	psi->lpVtbl->Release(psi);
	pfo->lpVtbl->Release(pfo);

	return ls_set_errno_hresult(hr);
#else
	return ls_delete(path);
#endif // LS_WINDOWS
}

int ls_shell_recycle(const char *path)
{
#if LS_WINDOWS
	WCHAR szPath[MAX_PATH];
	size_t len;
	HRESULT hr;
	IFileOperation *pfo = NULL;
	IShellItem *psi;

	len = ls_utf8_to_wchar_buf(path, szPath, MAX_PATH);
	if (len == -1)
		return -1;

	hr = CoCreateInstance(&CLSID_FileOperation, NULL, CLSCTX_ALL, &IID_IFileOperation, (void **)&pfo);
	if (FAILED(hr))
		return ls_set_errno_hresult(hr);

	hr = pfo->lpVtbl->SetOperationFlags(pfo, FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT | FOF_ALLOWUNDO);
	if (FAILED(hr))
	{
		pfo->lpVtbl->Release(pfo);
		return ls_set_errno_hresult(hr);
	}

	psi = ls_item_from_parsing_name(szPath, NULL);
	if (!psi)
	{
		pfo->lpVtbl->Release(pfo);
		return -1;
	}

	hr = pfo->lpVtbl->DeleteItem(pfo, psi, NULL);
	if (FAILED(hr))
	{
		psi->lpVtbl->Release(psi);
		pfo->lpVtbl->Release(pfo);
		return ls_set_errno_hresult(hr);
	}

	hr = pfo->lpVtbl->PerformOperations(pfo);

	psi->lpVtbl->Release(psi);
	pfo->lpVtbl->Release(pfo);

	return ls_set_errno_hresult(hr);
#else
	return ls_delete(path);
#endif // LS_WINDOWS
}
