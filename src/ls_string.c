#include <lysys/ls_string.h>

#include <lysys/ls_core.h>

#include "ls_native.h"

size_t ls_utf8_to_wchar_buf(const char *utf8, wchar_t *buf, size_t cchbuf)
{
#if LS_WINDOWS
	int cch;

	if (!utf8 || !buf != !cchbuf)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	if (cchbuf > INT32_MAX)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	cch = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, buf, (int)cchbuf);
	if (cch == 0)
		return ls_set_errno_win32(GetLastError());

	if (!cchbuf)
		return (size_t)cch;

	return (size_t)cch - 1; // exclude null terminator
#else
    size_t len;

    if (!utf8 || !buf != !cchbuf)
        return ls_set_errno(LS_INVALID_ARGUMENT);

    len = strlen(utf8) + 1;
    if (!buf)
        return len;

    if (cchbuf < len)
        return ls_set_errno(LS_BUFFER_TOO_SMALL);

    for (; *utf8; ++utf8, ++buf)
        *buf = (wchar_t)*utf8;
    *buf = 0;

    return len - 1;
#endif // LS_WINDOWS
}

wchar_t *ls_utf8_to_wchar(const char *utf8)
{
	wchar_t *wstr;
	size_t len;

	len = ls_utf8_to_wchar_buf(utf8, NULL, 0);
	if (len == -1)
		return NULL;

	wstr = ls_malloc(len * sizeof(wchar_t));
	if (!wstr)
		return NULL;

	len = ls_utf8_to_wchar_buf(utf8, wstr, len + 1);
	if (len == -1)
	{
		ls_free(wstr);
		return NULL;
	}

	return wstr;
}

size_t ls_wchar_to_utf8_buf(const wchar_t *wstr, char *buf, size_t cbbuf)
{
#if LS_WINDOWS
	int cb;

	if (!wstr || !buf != !cbbuf)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	if (cbbuf > INT32_MAX)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	cb = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, buf, (int)cbbuf, NULL, NULL);
	if (cb == 0)
		return ls_set_errno_win32(GetLastError());

	if (!cbbuf)
		return (size_t)cb;

	return (size_t)cb - 1; // exclude null terminator
#else
    size_t len;

    // crude implementation

    if (!wstr || !buf != !cbbuf)
        return ls_set_errno(LS_INVALID_ARGUMENT);

    len = wcslen(wstr) + 1;
    if (!buf)
        return len;

    if (cbbuf < len)
        return ls_set_errno(LS_BUFFER_TOO_SMALL);

    for (; *wstr; ++wstr, ++buf)
    {
        if (*wstr > 0x7f)
            *buf = '?';
        else
            *buf = (char)*wstr;
    }

    *buf = 0;

    return len - 1;
#endif // LS_WINDOWS
}

char *ls_wchar_to_utf8(const wchar_t *wstr)
{
	char *utf8;
	size_t len;

	len = ls_wchar_to_utf8_buf(wstr, NULL, 0);
	if (len == -1)
		return NULL;

	utf8 = ls_malloc(len);
	if (!utf8)
		return NULL;

	len = ls_wchar_to_utf8_buf(wstr, utf8, len);
	if (len == -1)
	{
		ls_free(utf8);
		return NULL;
	}

	return utf8;
}

