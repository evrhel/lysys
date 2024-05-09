#include <lysys/ls_font.h>

#include <lysys/ls_core.h>

#include <math.h>

#include "ls_util.h"
#include "ls_native.h"
#include "ls_sync_util.h"

#if LS_WINDOWS
#define FONT_REG_KEY L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts"
#endif // LS_WINDOWS

struct string
{
	size_t size;	//!< The size of the string's value, including the null terminator
	char value[];	//!< The string value
};

#define STR_OFF (offsetof(struct string, value))

static int _cache_initialized = 0;
static map_t *_cache = NULL;
static ls_lock_t _cache_lock;

//! \brief Free the cache.
//! 
//! Called when the program exits to free the cache and associated resources.
static void ls_free_cache(void)
{
	lock_destroy(&_cache_lock);
	ls_map_destroy(_cache);
}

//! \brief Duplicate a string.
//! 
//! \param ent The string to duplicate
//! 
//! \return A pointer to the duplicated string, or NULL if an error occurred
static struct string *string_dup(struct string *ent)
{
	struct string *dup;

	dup = ls_malloc(STR_OFF + ent->size);
	if (!dup)
		return NULL;
	return memcpy(dup, ent, STR_OFF + ent->size);
}

//! \brief Reserve memory for a string.
//! 
//! \param len The length of the string, excluding the null terminator.
//! Space for the null terminator is also reserved.
//! 
//! \return A pointer to the string, or NULL if an error occurred
static struct string *string_reserve(size_t len)
{
	struct string *string;

	string = ls_malloc(STR_OFF + len + 1);
	if (!string)
		return NULL;

	string->size = len + 1;
	return string;
}

//! \brief Lock the cache.
//! 
//! \return 0 if the cache was successfully locked, -1 if an error occurred
static int ls_lock_cache(void)
{
	int rc;

	if (!_cache_initialized)
	{
		_cache = ls_map_create(
			(map_cmp_t)&strcmp,
			(map_free_t)&ls_free, (map_dup_t)&ls_strdup,
			(map_free_t)&ls_free, (map_dup_t)&string_dup);
		if (!_cache)
			return -1;

		rc = lock_init(&_cache_lock);
		if (rc == -1)
		{
			rc = _ls_errno;
			ls_map_destroy(_cache);
			_cache = NULL;
			return ls_set_errno(rc);
		}

		_cache_initialized = 1;
		atexit(&ls_free_cache);
	}

	lock_lock(&_cache_lock);
	return 0;
}

//! \brief Unlock the cache.
static void ls_unlock_cache(void)
{
	lock_unlock(&_cache_lock);
}

//! \brief Cache a value in the cache.
//! 
//! \param name The name of the value
//! \param value The value to cache, NULL to check if the value is cached
//! \param buf The buffer to store the value
//! \param buf_len The size of the buffer
//! 
//! \return The length of the value, excluding the null terminator. If buf is
//! NULL and buf_len is 0, the return value is the number of bytes required
//! to store the value, including the null terminator. If an error occurs, -1
//!	is returned.
static size_t ls_cache_string(const char *name, const struct string *path_str, char *buf, size_t buf_len)
{
	entry_t *entry;
	const struct string *str;

	// check if the string is already cached
	entry = ls_map_find(_cache, ANY_CPTR(name));
	if (!entry)
	{
		if (_ls_errno != LS_NOT_FOUND)
			return -1;

		if (!path_str)
			return ls_set_errno(LS_INVALID_ARGUMENT);

		entry = ls_map_insert(_cache, ANY_CPTR(name), ANY_CPTR(path_str));
		if (!entry)
			return -1;
	}

	// copy the string to the buffer
	str = entry->value.cptr;

	if (!buf)
		return str->size;

	if (buf_len < str->size)
		return ls_set_errno(LS_BUFFER_TOO_SMALL);

	memcpy(buf, str->value, str->size);
	return str->size - 1;
}

//! \brief Evaluate how well a path matches a name.
//! 
//! \param name The name being queried
//! \param path The path to evaluate against
//! 
//! \return A score between 0.0 and 1.0, where 1.0 is a perfect match
static double ls_score_path(const char *name, const char *path)
{
	size_t nlen, plen;
	char *match;

	match = strstr(path, name);
	if (!match)
		return 0.0;

	nlen = strlen(name);
	plen = strlen(path);

	return (double)nlen / plen;
}

#if LS_WINDOWS

//! \brief Open the font registry key.
//! 
//! \param plpValueName Pointer to the buffer to store the value name
//! \param pcbMaxValueNameSize Pointer to the size of the value name buffer
//! \param plpValueData Pointer to the buffer to store the value data
//! \param pcbMaxValueDataSize Pointer to the size of the value data buffer
//!	
//! \return The opened registry key, or NULL if an error occurred
static HKEY ls_open_font_key(LPWSTR *plpValueName, PDWORD pcbMaxValueNameSize, LPWSTR *plpValueData, PDWORD pcbMaxValueDataSize)
{
	HKEY hKey;
	LONG lResult;

	lResult = RegOpenKeyExW(HKEY_LOCAL_MACHINE, FONT_REG_KEY, 0, KEY_READ, &hKey);
	if (lResult != ERROR_SUCCESS)
	{
		ls_set_errno_win32(lResult);
		return NULL;
	}

	lResult = RegQueryInfoKeyW(
		hKey,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		pcbMaxValueNameSize, pcbMaxValueDataSize,
		NULL, NULL);
	if (lResult != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		ls_set_errno_win32(lResult);
		return NULL;
	}

	*plpValueName = ls_malloc(*pcbMaxValueNameSize * sizeof(WCHAR));
	if (!*plpValueName)
	{
		RegCloseKey(hKey);
		return NULL;
	}

	*plpValueData = ls_malloc(*pcbMaxValueDataSize * sizeof(WCHAR));
	if (!*plpValueData)
	{
		ls_free(*plpValueName);
		RegCloseKey(hKey);
		return NULL;
	}

	return hKey;
}

//! \brief Resolve a font path (relative to the Fonts directory) to a full path.
//! 
//! \param szFontPath The font path to resolve
//! \param szFullPath The buffer to store the full path
//! \param stLen The size of the buffer
//! 
//! \return The length of the full path, excluding the null terminator. If an
//! error occurs, -1 is returned.
static size_t ls_resolve_path(LPCWSTR szFontPath, PWSTR szFullPath, size_t stLen)
{
	static const WCHAR szFontsDir[] = L"\\Fonts\\";
	static const size_t stFontsDirLen = (sizeof(szFontsDir) / sizeof(WCHAR)) - 1;

	size_t stWinLen;
	PWSTR szEnd;
	size_t len;

#if SIZE_MAX > UINT_MAX
	if (stLen > UINT_MAX)
		return ls_set_errno(LS_OUT_OF_RANGE);
#endif // SIZE_MAX > UINT_MAX

	stWinLen = GetWindowsDirectoryW(szFullPath, (UINT)stLen);
	if (!stWinLen)
		return ls_set_errno_win32(GetLastError());

	szEnd = szFullPath + stWinLen;
	stLen -= stWinLen;

	len = wcslen(szFontPath);

	if (len + stFontsDirLen + 1 >= stLen)
		return ls_set_errno(LS_BUFFER_TOO_SMALL);

	memcpy(szEnd, szFontsDir, stFontsDirLen * sizeof(WCHAR));
	memcpy(szEnd + stFontsDirLen, szFontPath, (len + 1) * sizeof(WCHAR));

	return wcslen(szFullPath);
}

//! \brief ls_cache_string for wide strings.
static size_t ls_cache_pathw(const char *name, LPCWSTR szPath, char *buf, size_t buf_len)
{
	struct string *str;
	size_t utf8_len;
	size_t len;

	utf8_len = ls_wchar_to_utf8_buf(szPath, NULL, 0);
	if (utf8_len == -1)
		return -1;

	str = string_reserve(utf8_len);
	if (!str)
		return -1;
	
	// convert the path to UTF-8
	utf8_len = ls_wchar_to_utf8_buf(szPath, str->value, utf8_len);
	if (utf8_len == -1)
	{
		ls_free(str);
		return -1;
	}

	// cache the result
	len = ls_cache_string(name, str, buf, buf_len);

	// the string is copied - free the original
	ls_free(str);

	return len;
}

#endif // LS_WINDOWS

size_t ls_find_system_font(const char *name, char *path, size_t path_len)
{
#if LS_WINDOWS
	HKEY hKey;
	LONG lResult;
	DWORD cbMaxValueNameSize, cbMaxValueDataSize;
	DWORD dwValueIndex;
	LPWSTR lpValueName;
	LPWSTR lpValueData;
	DWORD cbValueNameSize, cbValueDataSize, dwValueType;
	double best_score;
	double score;
	size_t len;
	int rc;
	WCHAR szBestPath[MAX_PATH];
	WCHAR szFullPath[MAX_PATH];
	char *namelc;
	char szName[MAX_PATH];

	if (!name)
		return ls_set_errno(LS_INVALID_ARGUMENT);
	if (!path != !path_len)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	if (ls_lock_cache() == -1)
		return -1;

	// Check the cache
	len = ls_cache_string(name, NULL, path, path_len);
	if (len != -1)
	{
		ls_unlock_cache();
		return len;
	}

	rc = _ls_errno;
	ls_unlock_cache();

	if (_ls_errno != LS_NOT_FOUND)
		return ls_set_errno(rc);
	
	namelc = ls_strdup(name);
	if (!namelc)
		return -1;
	ls_strlower(namelc, -1);

	// Open the registry key
	hKey = ls_open_font_key(&lpValueName, &cbMaxValueNameSize, &lpValueData, &cbMaxValueDataSize);
	if (!hKey)
	{
		ls_free(namelc);
		return -1;
	}

	// Iterate over the font registry entries
	best_score = 0.0;
	dwValueIndex = 0;

	do
	{
		// Reset the buffer
		cbValueDataSize = cbMaxValueDataSize;
		cbValueNameSize = cbMaxValueNameSize;

		// Fetch the next font entry
		lResult = RegEnumValueW(
			hKey,
			dwValueIndex,
			lpValueName,
			&cbValueNameSize,
			0,
			&dwValueType,
			(LPBYTE)lpValueData,
			&cbValueDataSize);

		dwValueIndex++;

		// check if the entry is a string
		if (lResult != ERROR_SUCCESS || dwValueType != REG_SZ)
			continue;

		// require short path name
		if (cbValueDataSize >= MAX_PATH-1 || cbValueNameSize >= MAX_PATH-1)
			continue;

		// Null terminate and convert to lowercase UTF-8
		lpValueName[cbValueNameSize] = 0;
		len = ls_wchar_to_utf8_buf(lpValueName, szName, MAX_PATH);
		if (len == -1)
			continue;
		ls_strlower(szName, -1);

		// Check if this font is a better match
		score = ls_score_path(namelc, szName);
		if (score > best_score)
		{
			best_score = score;
			memcpy(szBestPath, lpValueData, cbValueDataSize);
		}
	} while (lResult != ERROR_NO_MORE_ITEMS);

	// Close the registry key
	ls_free(lpValueData);
	ls_free(lpValueName);
	RegCloseKey(hKey);
	ls_free(namelc);

	// Check if a font was found
	if (best_score == 0.0)
		return ls_set_errno(LS_NOT_FOUND);

	// Resolve full path
	len = ls_resolve_path(szBestPath, szFullPath, MAX_PATH);
	if (len == -1)
		return ls_set_errno(rc);

	rc = ls_lock_cache();
	if (rc == -1)
		return -1;

	// Cache the result
	len = ls_cache_pathw(name, szFullPath, path, path_len);
	if (len == -1)
	{
		rc = _ls_errno;
		ls_unlock_cache();
		return ls_set_errno(rc);
	}

	ls_unlock_cache();
	return len;
#else
	return ls_set_errno(LS_NOT_IMPLEMENTED);
#endif
}
