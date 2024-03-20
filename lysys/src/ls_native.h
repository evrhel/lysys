#ifndef _LS_NATIVE_H_
#define _LS_NATIVE_H_

#include <lysys/ls_defs.h>

#if LS_WINDOWS
#define NOMINMAX
#include <Windows.h>
#include <TlHelp32.h>
#include <Shlwapi.h>

// utf8 string to wchar_t string
// returns number of characters written to buf, including null terminator
int ls_utf8_to_wchar_buf(const char *utf8, LPWSTR buf, int cbbuf);

LPWSTR ls_utf8_to_wchar(const char *utf8);

// wchar_t string to utf8 string
// returns number of bytes written to buf, including null terminator
int ls_wchar_to_utf8_buf(LPCWSTR wstr, char *buf, int cbbuf);

char *ls_wchar_to_utf8(LPCWSTR wstr);

DWORD ls_protect_to_flags(int protect);
int ls_flags_to_protect(DWORD flProtect);

DWORD ls_get_access_rights(int access);
DWORD ls_get_flags_and_attributes(int access);

int ls_append_escaped(struct ls_buffer *buf, LPWSTR str);
LPWSTR ls_build_command_line(const char *path, const char *argv[]);
LPWSTR ls_build_environment(const char *envp[]);

#endif // LS_WINDOWS

#endif // _LS_NATIVE_H_
