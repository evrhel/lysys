#include <lysys/ls_clipboard.h>

#include <lysys/ls_core.h>

#include <string.h>

#include "ls_native.h"
#include "ls_sync_util.h"

#if LS_DARWIN
#include "ls_pasteboard.h"
#elif LS_LINUX
#include "ls_clipboard_x11.h"

static struct clipboard clipboard;
static int clipboard_initialized = 0;

// restrict to one failed attempt to initialize the clipboard
static int clipboard_unavail = 0;

//! \brief Called when the program exits to release the
//! clipboard instance.
static void clipboard_release_thunk(void)
{
	if (clipboard_initialized)
	{
		clipboard_initialized = 0;
		clipboard_release(&clipboard);
	}
}

//! \brief Get the clipboard instance.
//!
//! \return The clipboard instance, or NULL if the function fails.
static struct clipboard *get_clipboard(void)
{
	int rc;
	
	if (clipboard_unavail)
		return NULL;

	// initialize the clipboard instance, if not already done
	if (!clipboard_initialized)
	{
		rc = clipboard_init(&clipboard);
		if (rc == -1)
		{
			// clipboard is unavailable, likely in a headless
			// environment
			clipboard_unavail = 1;
			return NULL;
		}
		atexit(&clipboard_release_thunk);
		clipboard_initialized = 1;
	}

	return &clipboard;
}

#endif // LS_DARWIN

intptr_t ls_register_clipboard_format(const char *name)
{
#if LS_WINDOWS
	UINT id;
	LPWSTR lpName;

	if (!name)
		return -1;

	lpName = ls_utf8_to_wchar(name);
	if (!lpName)
		return -1;

	id = RegisterClipboardFormatW(lpName);
	if (!id)
	{
		ls_set_errno(win32_to_error(GetLastError()));
		ls_free(lpName);
		return -1;
	}

	ls_free(lpName);

	return (intptr_t)id;
#elif LS_DARWIN
	return ls_register_pasteboard_format(name);
#else
	struct clipboard *clipboard;

	clipboard = get_clipboard();
	if (!clipboard)
		return -1;
	return clipboard_register_format(clipboard, name);
#endif // LS_WINDOWS
}

int ls_set_clipboard_data(intptr_t fmt, const void *data, size_t cb)
{
#if LS_WINDOWS
	HGLOBAL hGlbl;
	LPVOID lpMem;
	UINT uiFormat;
	HANDLE hData;

	if (!data || cb == 0)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	if (!OpenClipboard(NULL))
		return ls_set_errno_win32(GetLastError());

	hGlbl = GlobalAlloc(GMEM_MOVEABLE, cb);
	if (!hGlbl)
	{
		ls_set_errno_win32(GetLastError());
		CloseClipboard();
		return -1;
	}

	lpMem = GlobalLock(hGlbl);
	if (!lpMem)
	{
		ls_set_errno_win32(GetLastError());
		GlobalFree(hGlbl);
		CloseClipboard();
		return -1;
	}

	memcpy(lpMem, data, cb);
	(void)GlobalUnlock(hGlbl);

	uiFormat = (UINT)fmt;
	hData = SetClipboardData(uiFormat, hGlbl);
	if (!hData)
	{
		ls_set_errno_win32(GetLastError());
		GlobalFree(hGlbl);
		CloseClipboard();
		return -1;
	}

	CloseClipboard();

	return 0;
#elif LS_DARWIN
	return ls_set_pasteboard_data(fmt, data, cb);
#else
	struct clipboard *clipboard;

	clipboard = get_clipboard();
	if (!clipboard)
		return -1;
	return clipboard_set_data(clipboard, fmt, data, cb);
#endif // LS_WINDOWS
}

int ls_set_clipboard_text(const char *text)
{
	if (!text)
		return ls_set_errno(LS_INVALID_ARGUMENT);
    return ls_set_clipboard_data(LS_CF_TEXT, text, strlen(text));
}

int ls_clear_clipboard_data(void)
{
#if LS_WINDOWS

	if (!OpenClipboard(NULL))
		return ls_set_errno_win32(GetLastError());

	if (!EmptyClipboard())
	{
		ls_set_errno_win32(GetLastError());
		CloseClipboard();
		return -1;
	}

	CloseClipboard();

	return 0;
#elif LS_DARWIN
	return ls_clear_pasteboard_data();
#else
	struct clipboard *clipboard;

	clipboard = get_clipboard();
	if (!clipboard)
		return -1;
	return clipboard_clear_data(clipboard);
#endif // LS_WINDOWS
}

size_t ls_get_clipboard_data(intptr_t fmt, void *data, size_t cb)
{
#if LS_WINDOWS
	HGLOBAL hGlbl;
	LPVOID lpMem;
	UINT uiFormat;
	SIZE_T stSize;

	if (!data != !cb)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	if (!OpenClipboard(NULL))
		return ls_set_errno(LS_INVALID_STATE);

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
		return ls_set_errno(LS_OUT_OF_MEMORY);
	}

	stSize = GlobalSize(hGlbl);
	if (stSize == 0)
	{
		ls_set_errno_win32(GetLastError());
		GlobalUnlock(hGlbl);
		CloseClipboard();
		return -1;
	}

	if (cb == 0)
	{
		GlobalUnlock(hGlbl);
		CloseClipboard();
		return stSize;
	}

	if (cb < stSize)
	{
		GlobalUnlock(hGlbl);
		CloseClipboard();
		return ls_set_errno(LS_BUFFER_TOO_SMALL);
	}

	memcpy(data, lpMem, stSize);

	GlobalUnlock(hGlbl);
	CloseClipboard();

	return stSize;
#elif LS_DARWIN
	return ls_get_pasteboard_data(fmt, data, cb);
#else
	struct clipboard *clipboard;

	clipboard = get_clipboard();
	if (!clipboard)
		return -1;
	return clipboard_get_data(clipboard, fmt, data, cb);
#endif // LS_WINDOWS
}
