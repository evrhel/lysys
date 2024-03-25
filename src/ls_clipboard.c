#include <lysys/ls_clipboard.h>

#include <lysys/ls_core.h>

#include <string.h>

#include "ls_native.h"

intptr_t ls_register_clipboard_format(const char *name)
{
#if LS_WINDOWS
	UINT id;
	LPWSTR lpName;

	lpName = ls_utf8_to_wchar(name);
	if (!lpName)
		return -1;

	id = RegisterClipboardFormatW(lpName);
	free(lpName);
	if (!id)
		return -1;

	return (intptr_t)id;
#elif LS_DARWIN
	return ls_register_pasteboard_format(name);
#endif
}

int ls_set_clipboard_data(intptr_t fmt, const void *data, size_t cb)
{
#if LS_WINDOWS
	HGLOBAL hGlbl;
	LPVOID lpMem;
	UINT uiFormat;
	HANDLE hData;

	if (!OpenClipboard(NULL))
		return -1;

	hGlbl = GlobalAlloc(GMEM_MOVEABLE, cb);
	if (!hGlbl)
	{
		CloseClipboard();
		return -1;
	}

	lpMem = GlobalLock(hGlbl);
	if (!lpMem)
	{
		GlobalFree(hGlbl);
		CloseClipboard();
		return -1;
	}

	memcpy(lpMem, data, cb);
	GlobalUnlock(hGlbl);

	uiFormat = (UINT)fmt;
	hData = SetClipboardData(uiFormat, hGlbl);
	if (!hData)
	{
		GlobalFree(hGlbl);
		CloseClipboard();
		return -1;
	}

	CloseClipboard();

	return 0;
#elif LS_DARWIN
	return ls_set_pasteboard_data(fmt, data, cb);
#endif
}

int ls_set_clipboard_text(const char *text)
{
    return ls_set_clipboard_data(LS_CF_TEXT, text, strlen(text));
}

int ls_clear_clipboard_data(void)
{
#if LS_WINDOWS
	if (!OpenClipboard(NULL))
		return -1;

	if (!EmptyClipboard())
	{
		CloseClipboard();
		return -1;
	}

	CloseClipboard();

	return 0;
#elif LS_DARWIN
	return ls_clear_pasteboard_data();
#endif
}

size_t ls_get_clipboard_data(intptr_t fmt, void *data, size_t cb)
{
#if LS_WINDOWS
	HGLOBAL hGlbl;
	LPVOID lpMem;
	UINT uiFormat;
	SIZE_T stSize;

	if (!OpenClipboard(NULL))
		return -1;

	uiFormat = (UINT)fmt;
	hGlbl = GetClipboardData(uiFormat);
	if (!hGlbl)
	{
		CloseClipboard();
		return 0; // not available in requested format
	}

	lpMem = GlobalLock(hGlbl);
	if (!lpMem)
	{
		CloseClipboard();
		return -1;
	}

	stSize = GlobalSize(hGlbl);
	if (cb == 0)
	{
		GlobalUnlock(hGlbl);
		CloseClipboard();
		return stSize;
	}

	if (cb < stSize)
		stSize = cb;

	memcpy(data, lpMem, stSize);

	GlobalUnlock(hGlbl);

	CloseClipboard();
	return stSize;
#elif LS_DARWIN
	return ls_get_pasteboard_data(fmt, data, cb);
#endif
}
