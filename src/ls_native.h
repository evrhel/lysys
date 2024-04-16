#ifndef _LS_NATIVE_H_
#define _LS_NATIVE_H_

#include <lysys/ls_defs.h>

#if LS_WINDOWS
#define NOMINMAX
#include <Windows.h>
#include <TlHelp32.h>
#include <Shlwapi.h>
#include <Shlobj.h>
#include <Pdh.h>
#include <PdhMsg.h>
#include <strsafe.h>
#include <Psapi.h>

typedef DWORD native_flags_t;

// at least Windows 2000
#if WINVER < _WIN32_WINNT_WIN2K
#error "Windows version too old"
#endif

// utf8 string to wchar_t string
// returns number of characters written to buf, including null terminator
int ls_utf8_to_wchar_buf(const char *utf8, LPWSTR buf, int cchbuf);

LPWSTR ls_utf8_to_wchar(const char *utf8);

// wchar_t string to utf8 string
// returns number of bytes written to buf, including null terminator
int ls_wchar_to_utf8_buf(LPCWSTR wstr, char *buf, int cbbuf);

char *ls_wchar_to_utf8(LPCWSTR wstr);

DWORD ls_get_access_rights(int access);
DWORD ls_get_flags_and_attributes(int access);

int ls_append_escaped(struct ls_buffer *buf, LPWSTR str);
LPWSTR ls_build_command_line(const char *path, const char *argv[]);
LPWSTR ls_build_environment(const char *envp[]);

#endif // LS_WINDOWS

#if LS_POSIX
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/time.h>

#if LS_DARWIN
#include <copyfile.h>
#else
#include <sys/sendfile.h>
#include <sys/inotify.h>
#endif // LS_DARWIN

typedef int native_flags_t;

#endif // LS_POSIX

int ls_access_to_oflags(int access);
int ls_create_to_oflags(int create);

#endif // _LS_NATIVE_H_

native_flags_t ls_protect_to_flags(int protect);
int ls_flags_to_protect(native_flags_t prot);
