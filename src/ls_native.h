#ifndef _LS_NATIVE_H_
#define _LS_NATIVE_H_

#include <lysys/ls_defs.h>
#include <lysys/ls_error.h>

#include <wchar.h>

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
#include <lmcons.h>
#include <UserEnv.h>
#include <tchar.h>

typedef HANDLE native_file_t;
typedef DWORD native_flags_t;

// at least Windows 2000
#if WINVER < _WIN32_WINNT_WIN2K
#error "Windows version too old"
#endif // WINVER

//! \brief Convert a UTF-8 string to a wide character string
//!
//! \param utf8 UTF-8 string
//! \param buf Buffer to write wide character string
//! \param cchbuf Size of buffer in characters
//! 
//! \return If buf and cchbuf are both non-zero, returns the number of characters
//! written to buf, excluding the null terminator. If buf and cchbuf are both 0,
//! returns the number of characters required to store the wide character string,
//! including the null terminator. If an error occurs, returns -1.
size_t ls_utf8_to_wchar_buf(const char *utf8, wchar_t *buf, size_t cchbuf);

wchar_t *ls_utf8_to_wchar(const char *utf8);

//! \brief Convert a wide character string to a UTF-8 string
//! 
//! \param wstr Wide character string
//! \param buf Buffer to write UTF-8 string
//! \param cbbuf Size of buffer in bytes
//! 
//! \return If buf and cbbuf are both non-zero, returns the number of bytes
//! written to buf, excluding the null terminator. If buf and cbbuf are both 0,
//! returns the number of bytes required to store the UTF-8 string, including
//! the null terminator. If an error occurs, returns -1.
size_t ls_wchar_to_utf8_buf(const wchar_t *wstr, char *buf, size_t cbbuf);

char *ls_wchar_to_utf8(const wchar_t *wstr);

int ls_match_string(LPCWSTR lpStr, LPCWSTR lpPattern);

DWORD ls_get_access_rights(int access);
DWORD ls_get_flags_and_attributes(int access);

int ls_append_escaped(struct ls_buffer *buf, LPWSTR str);
LPWSTR ls_build_command_line(const char *path, const char *argv[]);
LPWSTR ls_build_environment(const char *envp[]);

//! \brief Convert Win32 error code to an error code
//! 
//! \param err Win32 error code
//! 
//! \return Error code
int win32_to_error(DWORD err);

//! \brief Convert HRESULT to an error code
//! 
//! \param hr HRESULT
//! 
//! \return Error code
#define hresult_to_error(hr) (win32_to_error(HRESULT_CODE(hr)))

//! \brief Convert PDH error code to an error code
//! 
//! \param err PDH error code
//! 
//! \return Error code
#define pdh_to_error(err) (win32_to_error((DWORD)(err)))

#endif // LS_WINDOWS

#if LS_POSIX
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <wordexp.h>
#include <aio.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/utsname.h>

#if LS_DARWIN
#include <copyfile.h>
#include <sys/sysctl.h>
#include <sys/resource.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>
#include <mach/mach.h>
#else
#include <sys/sendfile.h>
#include <sys/inotify.h>
#include <sys/sysinfo.h>
#include <xcb/xcb.h>
#endif // LS_DARWIN

typedef int native_file_t;
typedef int native_flags_t;

//! \brief Convert errno to an error code
//!
//! \param err Error code
//!
//! \return Error code
int ls_errno_to_error(int err);

#endif // LS_POSIX

int ls_access_to_oflags(int access);
int ls_create_to_oflags(int create);

#endif // _LS_NATIVE_H_

native_flags_t ls_protect_to_flags(int protect);
int ls_flags_to_protect(native_flags_t prot);

native_file_t ls_resolve_file(ls_handle fh);

#if LS_DARWIN
int kr_to_error(kern_return_t kr);
#endif // LS_DARWIN

extern LS_THREADLOCAL int _ls_errno;

//! \brief Set errno and return -1 or 0 conditionally
//! 
//! \param _err Error code
//! 
//! \return -1 if the error code is non-zero, 0 otherwise
#define ls_set_errno(_err) (_ls_errno = (_err), _ls_errno == 0 ? 0 : -1)

#if LS_WINDOWS
#define ls_set_errno_win32(err) ls_set_errno(win32_to_error(err))
#define ls_set_errno_hresult(err) ls_set_errno(hresult_to_error(err))
#define ls_set_errno_pdh(err) ls_set_errno(pdh_to_error(err))
#else

#define ls_set_errno_errno(err) ls_set_errno(ls_errno_to_error(err))

#if LS_DARWIN
#define ls_set_errno_kr(err) ls_set_errno(kr_to_error(err))
#endif // LS_DARWIN

#endif // LS_WINDOWS
