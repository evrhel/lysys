#include "ls_native.h"

#include <stdlib.h>

#include <lysys/ls_memory.h>
#include <lysys/ls_file.h>
#include <lysys/ls_core.h>
#include <lysys/ls_proc.h>
#include <lysys/ls_string.h>

#include "ls_buffer.h"

#ifdef LS_WINDOWS

int ls_match_string(LPCWSTR lpStr, LPCWSTR lpPattern)
{
	if (!*lpPattern)
		return !!*lpStr;

	if (*lpPattern == '*')
	{
		for (; *lpStr; ++lpStr)
		{
			if (!ls_match_string(lpStr, lpPattern + 1))
				return 0;
		}
		return ls_match_string(lpStr, lpPattern + 1);
	}

	for (; *lpPattern && *lpStr; ++lpPattern)
	{
		if (*lpPattern == '*')
		{
			if (!ls_match_string(lpStr, lpPattern))
				return 0;
		}
		else if (*lpPattern != '?' && *lpPattern != *lpStr)
			return 1;
		else
			lpStr++;
	}

	return *lpPattern && *lpStr; // both strings must be at the end
}

DWORD ls_get_access_rights(int access)
{
	DWORD dwDesiredAccess = 0;

	if (access & LS_FILE_READ)
		dwDesiredAccess |= GENERIC_READ;

	if (access & LS_FILE_WRITE)
		dwDesiredAccess |= GENERIC_WRITE;

	if (access & LS_FILE_EXECUTE)
		dwDesiredAccess |= GENERIC_EXECUTE;

	return dwDesiredAccess;
}

DWORD ls_get_flags_and_attributes(int access)
{
	DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;

	if (access & LS_FLAG_ASYNC)
		dwFlagsAndAttributes |= FILE_FLAG_OVERLAPPED;

	if (access & LS_FLAG_RANDOM)
		dwFlagsAndAttributes |= FILE_FLAG_RANDOM_ACCESS;

	if (access & LS_FLAG_SEQUENTIAL)
		dwFlagsAndAttributes |= FILE_FLAG_SEQUENTIAL_SCAN;

	return dwFlagsAndAttributes;
}

static int is_whitespace(WCHAR c)
{
	return c == L' ' || c == L'\t' || c == L'\n' || c == L'\r';
}

static int has_whitespace(LPWSTR lpStr)
{
	WCHAR c;

	while ((c = *lpStr))
	{
		if (is_whitespace(c))
			return 1;
		lpStr++;
	}

	return 0;
}

int ls_append_escaped(struct ls_buffer *buf, LPWSTR str)
{
	WCHAR c;

	if (!has_whitespace(str))
	{
		if (ls_buffer_write(buf, str, wcslen(str) * sizeof(WCHAR)) == -1) return -1;
		return 0;
	}

	if (ls_buffer_put_wchar(buf, L'\"') == -1) return -1;

	for (; *str; ++str)
	{
		c = *str;
		switch (c)
		{
		case '\"':
		case '\\':
			if (ls_buffer_put_wchar(buf, L'\\') == -1) return -1;
			if (ls_buffer_put_wchar(buf, c) == -1) return -1;
			break;
		case '\t':
			if (ls_buffer_put_wchar(buf, L'\\') == -1) return -1;
			if (ls_buffer_put_wchar(buf, L't') == -1) return -1;
			break;
		case '\n':
			if (ls_buffer_put_wchar(buf, L'\\') == -1) return -1;
			if (ls_buffer_put_wchar(buf, L'n') == -1) return -1;
			break;
		case '\r':
			if (ls_buffer_put_wchar(buf, L'\\') == -1) return -1;
			if (ls_buffer_put_wchar(buf, L'r') == -1) return -1;
			break;
		default:
			if (ls_buffer_put_wchar(buf, c) == -1) return -1;
			break;
		}
	}

	if (ls_buffer_put_wchar(buf, L'\"') == -1) return -1;

	return 0;
}

LPWSTR ls_build_command_line(const char *path, const char *argv[])
{
	LPWSTR wpath;
	LPWSTR wargv[LS_MAX_ARGV];
	LPWSTR *warg;
	int num_args = 0;
	struct ls_buffer buf;
	size_t len;
	int rc = 0;
	int err;

	_ls_errno = 0;

	if (!path)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	ZeroMemory(wargv, sizeof(wargv));
	ZeroMemory(&buf, sizeof(buf));

	// convert path to wide char
	wpath = ls_utf8_to_wchar(path);
	if (!wpath) goto done;
	len = wcslen(wpath) + 1; // 1 for null terminator

	// convert argv to wide char
	if (argv)
	{
		while (*argv)
		{
			if (num_args >= LS_MAX_ARGV) goto done;

			wargv[num_args] = ls_utf8_to_wchar(*argv);
			if (!wargv[num_args]) goto done;

			len += wcslen(wargv[num_args]) + 3; // 3 for space and quotes

			num_args++;
			argv++;
		}
	}

	// 1024 should be enough for most cases, will grow if needed
	rc = ls_buffer_reserve(&buf, 1024);
	if (rc == -1) goto done;

	// append path
	ls_append_escaped(&buf, wpath);
	if (rc == -1) goto done;

	// append args
	for (warg = wargv; warg < wargv + num_args; warg++)
	{
		rc = ls_buffer_put_wchar(&buf, L' ');
		if (rc == -1) goto done;

		rc = ls_append_escaped(&buf, *warg);
		if (rc == -1) goto done;
	}

	// null terminator
	rc = ls_buffer_put_wchar(&buf, L'\0');

done:
	err = _ls_errno; // remember error code

	if (rc == -1)
		ls_buffer_release(&buf);

	while (num_args > 1)
		ls_free(wargv[--num_args]);

	ls_free(wpath);

	ls_set_errno(err);
	return (LPWSTR)buf.data;
}

LPWSTR ls_build_environment(const char *envp[])
{
	struct ls_buffer buf;
	size_t len;
	int rc;

	LPWSTR str = NULL;

	if (!envp)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	ZeroMemory(&buf, sizeof(buf));

	while (*envp)
	{
		str = ls_utf8_to_wchar(*envp);
		if (!str) goto failure;

		len = wcslen(str);

		rc = ls_buffer_write(&buf, str, len * sizeof(WCHAR));
		if (rc == -1) goto failure;

		rc = ls_buffer_put_wchar(&buf, L'\0');
		if (rc == -1) goto failure;

		ls_free(str);

		envp++;
	}

	str = NULL;

	rc = ls_buffer_put_wchar(&buf, L'\0');
	if (rc == -1) goto failure;

	return (LPWSTR)buf.data;

failure:
	ls_free(str);
	ls_buffer_release(&buf);
	return NULL;
}

int win32_to_error(DWORD err)
{
	switch (err)
	{
	default: hresult_to_error(HRESULT_FROM_WIN32(err));
	case 0: return 0;
	case ERROR_INVALID_FUNCTION: return LS_INVALID_ARGUMENT;
	case ERROR_FILE_NOT_FOUND: return LS_FILE_NOT_FOUND;
	case ERROR_PATH_NOT_FOUND: return LS_PATH_NOT_FOUND;
	case ERROR_TOO_MANY_OPEN_FILES: return LS_OUT_OF_MEMORY;
	case ERROR_ACCESS_DENIED: return LS_ACCESS_DENIED;
	case ERROR_INVALID_HANDLE: return LS_INVALID_HANDLE;
	case ERROR_ARENA_TRASHED: return LS_OUT_OF_MEMORY;
	case ERROR_NOT_ENOUGH_MEMORY: return LS_OUT_OF_MEMORY;
	case ERROR_INVALID_BLOCK: return LS_OUT_OF_MEMORY;
	case ERROR_BAD_ENVIRONMENT: return LS_INVALID_ARGUMENT;
	case ERROR_BAD_FORMAT: return LS_INVALID_IMAGE;
	case ERROR_INVALID_ACCESS: return LS_ACCESS_DENIED;
	case ERROR_INVALID_DATA: return LS_INVALID_ARGUMENT;
	case ERROR_OUTOFMEMORY: return LS_OUT_OF_MEMORY;
	case ERROR_INVALID_DRIVE: return LS_INVALID_PATH;
	case ERROR_CURRENT_DIRECTORY: return LS_INVALID_PATH;
	case ERROR_NOT_SAME_DEVICE: return LS_NOT_SUPPORTED;
	case ERROR_NO_MORE_FILES: return LS_NO_MORE_FILES;
	case ERROR_BAD_UNIT: return LS_INVALID_HANDLE;
	case ERROR_NOT_READY: return LS_NOT_READY;
	case ERROR_BAD_COMMAND: return LS_INVALID_ARGUMENT;
	case ERROR_CRC: return LS_INVALID_STATE;
	case ERROR_BAD_LENGTH: return LS_INVALID_ARGUMENT;
	case ERROR_SEEK: return LS_INVALID_ARGUMENT;
	case ERROR_NOT_DOS_DISK: return LS_INVALID_PATH;
	case ERROR_SECTOR_NOT_FOUND: return LS_NOT_FOUND;

	case ERROR_WRITE_FAULT: return LS_IO_ERROR;
	case ERROR_READ_FAULT: return LS_IO_ERROR;
	case ERROR_GEN_FAILURE: return LS_UNKNOWN_ERROR;
	case ERROR_SHARING_VIOLATION: return LS_SHARING_VIOLATION;
	case ERROR_LOCK_VIOLATION: return LS_SHARING_VIOLATION;

	case ERROR_SHARING_BUFFER_EXCEEDED: return LS_OUT_OF_MEMORY;
	case ERROR_HANDLE_EOF: return LS_END_OF_FILE;
	case ERROR_HANDLE_DISK_FULL: return LS_DISK_FULL;
	case ERROR_NOT_SUPPORTED: return LS_NOT_SUPPORTED;
	case ERROR_REM_NOT_LIST: return LS_NOT_FOUND;
	case ERROR_DUP_NAME: return LS_ALREADY_EXISTS;
	case ERROR_BAD_NETPATH: return LS_INVALID_PATH;
	case ERROR_NETWORK_BUSY: return LS_BUSY;
	case ERROR_DEV_NOT_EXIST: return LS_NOT_FOUND;
	case ERROR_TOO_MANY_CMDS: return LS_OUT_OF_MEMORY;
	case ERROR_ADAP_HDW_ERR: return LS_IO_ERROR;
	case ERROR_BAD_NET_RESP: return LS_IO_ERROR;
	case ERROR_UNEXP_NET_ERR: return LS_IO_ERROR;

	case ERROR_NETNAME_DELETED: return LS_NOT_FOUND;
	case ERROR_NETWORK_ACCESS_DENIED: return LS_ACCESS_DENIED;
	case ERROR_BAD_DEV_TYPE: return LS_INVALID_HANDLE;
	case ERROR_BAD_NET_NAME: return LS_INVALID_PATH;
	case ERROR_TOO_MANY_NAMES: return LS_OUT_OF_MEMORY;
	case ERROR_TOO_MANY_SESS: return LS_OUT_OF_MEMORY;
	case ERROR_SHARING_PAUSED: return LS_BUSY;
	case ERROR_REQ_NOT_ACCEP: return LS_NOT_READY;
	case ERROR_REDIR_PAUSED: return LS_BUSY;
	case ERROR_FILE_EXISTS: return LS_ALREADY_EXISTS;
	case ERROR_CANNOT_MAKE: return LS_ACCESS_DENIED;

	case ERROR_OUT_OF_STRUCTURES: return LS_OUT_OF_MEMORY;

	case ERROR_INVALID_PARAMETER: return LS_INVALID_ARGUMENT;
	case ERROR_NET_WRITE_FAULT: return LS_IO_ERROR;
	case ERROR_NO_PROC_SLOTS: return LS_OUT_OF_MEMORY;

	case ERROR_DISK_CHANGE: return LS_INVALID_STATE;
	case ERROR_DRIVE_LOCKED: return LS_ACCESS_DENIED;
	case ERROR_BROKEN_PIPE: return LS_BAD_PIPE;
	case ERROR_OPEN_FAILED: return LS_NOT_FOUND;
	case ERROR_BUFFER_OVERFLOW: return LS_INVALID_PATH;
	case ERROR_DISK_FULL: return LS_DISK_FULL;
	case ERROR_NO_MORE_SEARCH_HANDLES: return LS_OUT_OF_MEMORY;
	case ERROR_INVALID_TARGET_HANDLE: return LS_INVALID_HANDLE;

	case ERROR_BAD_DRIVER_LEVEL: return LS_INVALID_ARGUMENT;
	case ERROR_CALL_NOT_IMPLEMENTED: return LS_NOT_IMPLEMENTED;
	case ERROR_SEM_TIMEOUT: return LS_TIMEDOUT;
	case ERROR_INSUFFICIENT_BUFFER: return LS_BUFFER_TOO_SMALL;
	case ERROR_INVALID_NAME: return LS_INVALID_PATH;

	case ERROR_NO_VOLUME_LABEL: return LS_NOT_FOUND;
	case ERROR_MOD_NOT_FOUND: return LS_NOT_FOUND;
	case ERROR_PROC_NOT_FOUND: return LS_NOT_FOUND;
	case ERROR_WAIT_NO_CHILDREN: return LS_NOT_FOUND;
	case ERROR_DIRECT_ACCESS_HANDLE: return LS_INVALID_HANDLE;
	case ERROR_NEGATIVE_SEEK: return LS_OUT_OF_RANGE;
	case ERROR_SEEK_ON_DEVICE: return LS_INVALID_STATE;

	case ERROR_DIR_NOT_ROOT: return LS_INVALID_PATH;
	case ERROR_DIR_NOT_EMPTY: return LS_INVALID_STATE;

	case ERROR_PATH_BUSY: return LS_BUSY;

	case ERROR_DISCARDED: return LS_INVALID_STATE;
	case ERROR_NOT_LOCKED: return LS_INVALID_STATE;
	case ERROR_BAD_THREADID_ADDR: return LS_INVALID_ARGUMENT;
	case ERROR_BAD_ARGUMENTS: return LS_INVALID_ARGUMENT;
	case ERROR_BAD_PATHNAME: return LS_INVALID_PATH;
	case ERROR_SIGNAL_PENDING: return LS_BUSY;
	case ERROR_MAX_THRDS_REACHED: return LS_OUT_OF_MEMORY;
	case ERROR_LOCK_FAILED: return LS_ACCESS_DENIED;
	case ERROR_BUSY: return LS_BUSY;
	case ERROR_DEVICE_SUPPORT_IN_PROGRESS: return LS_BUSY;
	case ERROR_CANCEL_VIOLATION: return LS_INVALID_STATE;

	case ERROR_INVALID_FLAG_NUMBER: return LS_INVALID_ARGUMENT;
	case ERROR_INVALID_MODULETYPE: return LS_INVALID_IMAGE;
	case ERROR_INVALID_EXE_SIGNATURE: return LS_INVALID_IMAGE;
	case ERROR_EXE_MARKED_INVALID: return LS_INVALID_IMAGE;
	case ERROR_BAD_EXE_FORMAT: return LS_INVALID_IMAGE;
	case ERROR_ITERATED_DATA_EXCEEDS_64k: return LS_INVALID_IMAGE;
	case ERROR_INVALID_MINALLOCSIZE: return LS_INVALID_IMAGE;
	case ERROR_DYNLINK_FROM_INVALID_RING: return LS_INVALID_IMAGE;
	case ERROR_IOPL_NOT_ENABLED: return LS_ACCESS_DENIED;
	case ERROR_INVALID_SEGDPL: return LS_INVALID_IMAGE;
	case ERROR_AUTODATASEG_EXCEEDS_64k: return LS_INVALID_IMAGE;
	case ERROR_RING2SEG_MUST_BE_MOVABLE: return LS_INVALID_IMAGE;
	case ERROR_RELOC_CHAIN_XEEDS_SEGLIM: return LS_INVALID_IMAGE;
	case ERROR_INFLOOP_IN_RELOC_CHAIN: return LS_INVALID_IMAGE;
	case ERROR_ENVVAR_NOT_FOUND: return LS_NOT_FOUND;
	case ERROR_NO_SIGNAL_SENT: return LS_INVALID_STATE;
	case ERROR_FILENAME_EXCED_RANGE: return LS_INVALID_PATH;
	case ERROR_RING2_STACK_IN_USE: return LS_INVALID_STATE;
	case ERROR_META_EXPANSION_TOO_LONG: return LS_INVALID_ARGUMENT;
	case ERROR_INVALID_SIGNAL_NUMBER: return LS_INVALID_ARGUMENT;
	case ERROR_THREAD_1_INACTIVE: return LS_INVALID_STATE;
	case ERROR_LOCKED: return LS_ACCESS_DENIED;
	case ERROR_TOO_MANY_MODULES: return LS_OUT_OF_MEMORY;
	case ERROR_NESTING_NOT_ALLOWED: return LS_INVALID_STATE;
	case ERROR_EXE_MACHINE_TYPE_MISMATCH: return LS_INVALID_IMAGE;
	case ERROR_EXE_CANNOT_MODIFY_SIGNED_BINARY: return LS_ACCESS_DENIED;
	case ERROR_EXE_CANNOT_MODIFY_STRONG_SIGNED_BINARY: return LS_ACCESS_DENIED;
	case ERROR_FILE_CHECKED_OUT: return LS_BUSY;
	case ERROR_CHECKOUT_REQUIRED: return LS_BUSY;
	case ERROR_BAD_FILE_TYPE: return LS_INVALID_STATE;
	case ERROR_FILE_TOO_LARGE: return LS_DISK_FULL;
	case ERROR_FORMS_AUTH_REQUIRED: return LS_ACCESS_DENIED;
	case ERROR_VIRUS_DELETED: return LS_ACCESS_DENIED;
	case ERROR_PIPE_LOCAL: return LS_BAD_PIPE;
	case ERROR_BAD_PIPE: return LS_BAD_PIPE;
	case ERROR_PIPE_BUSY: return LS_BUSY;
	case ERROR_NO_DATA: return LS_NO_DATA;
	case ERROR_VC_DISCONNECTED: return LS_INVALID_STATE;
	case ERROR_INVALID_EA_NAME: return LS_INVALID_ARGUMENT;
	case ERROR_EA_LIST_INCONSISTENT: return LS_INVALID_STATE;
	case WAIT_TIMEOUT: return LS_TIMEDOUT;
	case ERROR_NO_MORE_ITEMS: return LS_NO_DATA;
	case ERROR_CANNOT_COPY: return LS_ACCESS_DENIED;
	case ERROR_DIRECTORY: return LS_INVALID_PATH;
	case ERROR_EAS_DIDNT_FIT: return LS_OUT_OF_MEMORY;
	case ERROR_EA_FILE_CORRUPT: return LS_INVALID_STATE;
	case ERROR_EA_TABLE_FULL: return LS_OUT_OF_MEMORY;
	case ERROR_INVALID_EA_HANDLE: return LS_INVALID_HANDLE;
	case ERROR_EAS_NOT_SUPPORTED: return LS_NOT_SUPPORTED;
	case ERROR_NOT_OWNER: return LS_ACCESS_DENIED;

	case ERROR_PARTIAL_COPY: return LS_IO_ERROR;
	case ERROR_OPLOCK_NOT_GRANTED: return LS_ACCESS_DENIED;
	case ERROR_INVALID_OPLOCK_PROTOCOL: return LS_INVALID_STATE;
	case ERROR_DISK_TOO_FRAGMENTED: return LS_INVALID_STATE;
	case ERROR_DELETE_PENDING: return LS_INVALID_STATE;

	case ERROR_INVALID_LOCK_RANGE: return LS_OUT_OF_RANGE;
	case ERROR_IMAGE_SUBSYSTEM_NOT_PRESENT: return LS_INVALID_IMAGE;
	case ERROR_NOTIFICATION_GUID_ALREADY_DEFINED: return LS_ALREADY_EXISTS;

	case ERROR_NOT_ALLOWED_ON_SYSTEM_FILE: return LS_ACCESS_DENIED;
	case ERROR_DISK_RESOURCES_EXHAUSTED: return LS_DISK_FULL;
	case ERROR_INVALID_TOKEN: return LS_INVALID_HANDLE;

	case ERROR_DEVICE_UNREACHABLE: return LS_INVALID_STATE;
	case ERROR_DEVICE_NO_RESOURCES: return LS_OUT_OF_MEMORY;
	case ERROR_DATA_CHECKSUM_ERROR: return LS_INVALID_STATE;
	case ERROR_INTERMIXED_KERNEL_EA_OPERATION: return LS_INVALID_STATE;
	case ERROR_FILE_LEVEL_TRIM_NOT_SUPPORTED: return LS_NOT_SUPPORTED;
	case ERROR_OFFSET_ALIGNMENT_VIOLATION: return LS_INVALID_ARGUMENT;
	case ERROR_INVALID_FIELD_IN_PARAMETER_LIST: return LS_INVALID_ARGUMENT;
	case ERROR_OPERATION_IN_PROGRESS: return LS_BUSY;
	case ERROR_BAD_DEVICE_PATH: return LS_INVALID_PATH;
	case ERROR_TOO_MANY_DESCRIPTORS: return LS_OUT_OF_MEMORY;

	case ERROR_NOT_REDUNDANT_STORAGE: return LS_NOT_SUPPORTED;
	case ERROR_RESIDENT_FILE_NOT_SUPPORTED: return LS_NOT_SUPPORTED;
	case ERROR_COMPRESSED_FILE_NOT_SUPPORTED: return LS_NOT_SUPPORTED;
	case ERROR_DIRECTORY_NOT_SUPPORTED: return LS_NOT_SUPPORTED;
	case ERROR_NOT_READ_FROM_COPY: return LS_IO_ERROR;

	case ERROR_MAX_SESSIONS_REACHED: return LS_OUT_OF_MEMORY;
	case ERROR_THREAD_MODE_ALREADY_BACKGROUND: return LS_INVALID_STATE;
	case ERROR_THREAD_MODE_NOT_BACKGROUND: return LS_INVALID_STATE;
	case ERROR_PROCESS_MODE_ALREADY_BACKGROUND: return LS_INVALID_STATE;
	case ERROR_PROCESS_MODE_NOT_BACKGROUND: return LS_INVALID_STATE;
	case ERROR_INVALID_ADDRESS: return LS_INVALID_ARGUMENT;

	case ERROR_USER_PROFILE_LOAD: return LS_INVALID_STATE;
	case ERROR_ARITHMETIC_OVERFLOW: return LS_OUT_OF_RANGE;
	case ERROR_PIPE_CONNECTED: return LS_BUSY;
	case ERROR_PIPE_LISTENING: return LS_BUSY;

	case ERROR_ILLEGAL_CHARACTER: return LS_INVALID_ENCODING;
	case ERROR_UNDEFINED_CHARACTER: return LS_INVALID_ENCODING;

	case ERROR_INSUFFICIENT_POWER: return LS_ACCESS_DENIED;

	case ERROR_FILE_SYSTEM_LIMITATION: return LS_NOT_SUPPORTED;

	case ERROR_ELEVATION_REQUIRED: return LS_ACCESS_DENIED;

	case ERROR_COMPRESSION_DISABLED: return LS_NOT_SUPPORTED;

	case ERROR_NOT_CAPABLE: return LS_NOT_SUPPORTED;

	case ERROR_OPERATION_ABORTED: return LS_CANCELED;
	case ERROR_IO_INCOMPLETE: return LS_IO_ERROR;
	case ERROR_IO_PENDING: return LS_BUSY;
	case ERROR_NOACCESS: return LS_ACCESS_DENIED;

	case ERROR_CAN_NOT_COMPLETE: return LS_INVALID_STATE;
	case ERROR_INVALID_FLAGS: return LS_INVALID_ARGUMENT;
	case ERROR_UNRECOGNIZED_VOLUME: return LS_INVALID_PATH;
	case ERROR_FILE_INVALID: return LS_INVALID_PATH;

	case ERROR_POSSIBLE_DEADLOCK: return LS_DEADLOCK;
	case ERROR_MAPPED_ALIGNMENT: return LS_INVALID_ARGUMENT;

	case ERROR_TOO_MANY_LINKS: return LS_NOT_SUPPORTED;
	case ERROR_OLD_WIN_VERSION: return LS_INVALID_IMAGE;
	case ERROR_APP_WRONG_OS: return LS_INVALID_IMAGE;
	case ERROR_SINGLE_INSTANCE_APP: return LS_ACCESS_DENIED;
	case ERROR_RMODE_APP: return LS_INVALID_IMAGE;
	case ERROR_INVALID_DLL: return LS_INVALID_IMAGE;
	case ERROR_NO_ASSOCIATION: return LS_NOT_FOUND;
	case ERROR_DDE_FAIL: return LS_INVALID_STATE;
	case ERROR_DLL_NOT_FOUND: return LS_NOT_FOUND;
	case ERROR_NO_MORE_USER_HANDLES: return LS_OUT_OF_MEMORY;
	case ERROR_MESSAGE_SYNC_ONLY: return LS_INVALID_STATE;

	case ERROR_DEVICE_NOT_CONNECTED: return LS_INVALID_STATE;
	case ERROR_NOT_FOUND: return LS_NOT_FOUND;
	case ERROR_NO_MATCH: return LS_NOT_FOUND;

	case ERROR_NO_VOLUME_ID: return LS_NOT_FOUND;

	case ERROR_CANCELLED: return LS_CANCELED;

	case ERROR_REQUEST_ABORTED: return LS_INTERRUPTED;

	case ERROR_RETRY: return LS_INTERRUPTED;

	case ERROR_ALREADY_FIBER: return LS_INVALID_STATE;
	case ERROR_ALREADY_THREAD: return LS_INVALID_STATE;

	case ERROR_IMPLEMENTATION_LIMIT: return LS_NOT_SUPPORTED;
	case ERROR_PROCESS_IS_PROTECTED: return LS_ACCESS_DENIED;

	case ERROR_DISK_QUOTA_EXCEEDED: return LS_DISK_FULL;
	case ERROR_CONTENT_BLOCKED: return LS_ACCESS_DENIED;

	case ERROR_TIMEOUT: return LS_TIMEDOUT;
	case ERROR_INCORRECT_SIZE: return LS_INVALID_ARGUMENT;
	case ERROR_SYMLINK_CLASS_DISABLED: return LS_NOT_SUPPORTED;
	case ERROR_SYMLINK_NOT_SUPPORTED: return LS_NOT_SUPPORTED;

	case ERROR_EVENTLOG_FILE_CORRUPT: return LS_INVALID_STATE;
	case ERROR_EVENTLOG_CANT_START: return LS_INVALID_STATE;
	case ERROR_LOG_FILE_FULL: return LS_DISK_FULL;
	case ERROR_EVENTLOG_FILE_CHANGED: return LS_INVALID_STATE;

	case ERROR_INVALID_HANDLE_STATE: return LS_INVALID_STATE;

	case ERROR_UNSUPPORTED_TYPE: return LS_NOT_SUPPORTED;

	case ERROR_INVALID_PRIORITY: return LS_INVALID_ARGUMENT;

	case ERROR_FILE_OFFLINE: return LS_INVALID_STATE;

	case ERROR_NOT_A_REPARSE_POINT: return LS_INVALID_STATE;
	case ERROR_REPARSE_TAG_INVALID: return LS_INVALID_STATE;

	case ERROR_INVALID_STATE: return LS_INVALID_STATE;

	case ERROR_ENCRYPTION_FAILED: return LS_ACCESS_DENIED;
	case ERROR_DECRYPTION_FAILED: return LS_ACCESS_DENIED;
	case ERROR_FILE_ENCRYPTED: return LS_ACCESS_DENIED;

	case ERROR_FILE_READ_ONLY: return LS_ACCESS_DENIED;

	case ERROR_HANDLE_NO_LONGER_VALID: return LS_INVALID_HANDLE;
	}
}

#endif // LS_WINDOWS

#if LS_POSIX

int ls_errno_to_error(int err)
{
	switch (err)
	{
	default: return LS_UNKNOWN_ERROR;
	case 0: return 0;
	case EPERM: return LS_ACCESS_DENIED;
	case ENOENT: return LS_FILE_NOT_FOUND;
	case ESRCH: return LS_NOT_FOUND;
	case EINTR: return LS_INTERRUPTED;
	case EIO: return LS_IO_ERROR;
	case ENXIO: return LS_NOT_FOUND;
	case E2BIG: return LS_INVALID_ARGUMENT;
	case ENOEXEC: return LS_INVALID_ARGUMENT;
	case EBADF: return LS_INVALID_HANDLE;
	case ECHILD: return LS_NOT_FOUND;
	case EAGAIN: return LS_NOT_READY;
	case ENOMEM: return LS_OUT_OF_MEMORY;
	case EACCES: return LS_ACCESS_DENIED;
	case EFAULT: return LS_INVALID_ARGUMENT;
	case ENOTBLK: return LS_INVALID_ARGUMENT;
	case EBUSY: return LS_BUSY;
	case EEXIST: return LS_ALREADY_EXISTS;
	case EXDEV: return LS_NOT_SUPPORTED;
	case ENODEV: return LS_NOT_FOUND;
	case ENOTDIR: return LS_INVALID_PATH;
	case EISDIR: return LS_ACCESS_DENIED;
	case EINVAL: return LS_INVALID_ARGUMENT;
	case ENFILE: return LS_OUT_OF_MEMORY;
	case EMFILE: return LS_OUT_OF_MEMORY;
	case ENOTTY: return LS_INVALID_HANDLE;
	case ETXTBSY: return LS_BUSY;
	case EFBIG: return LS_OUT_OF_RANGE;
	case ENOSPC: return LS_DISK_FULL;
	case ESPIPE: return LS_INVALID_STATE;
	case EROFS: return LS_ACCESS_DENIED;
	case EMLINK: return LS_OUT_OF_MEMORY;
	case EPIPE: return LS_BAD_PIPE;
	case EDOM: return LS_INVALID_ARGUMENT;
	case ERANGE: return LS_OUT_OF_RANGE;

	case EDEADLK: return LS_DEADLOCK;
	case ENAMETOOLONG: return LS_INVALID_PATH;

	case ELOOP: return LS_INVALID_PATH;

	case ENOSTR: return LS_INVALID_STATE;
	case ENODATA: return LS_NO_DATA;

#ifdef ENOPKG
	case ENOPKG: return LS_NOT_FOUND;
#endif // ENOPKG

	case EBADMSG: return LS_INVALID_STATE;
	case EOVERFLOW: return LS_OUT_OF_RANGE;

#ifdef EBADFD
	case EBADFD: return LS_INVALID_HANDLE;
#endif // EBADFD

	case EOPNOTSUPP: return LS_NOT_SUPPORTED;

	case ENETDOWN: return LS_INVALID_STATE;
	case ENETUNREACH: return LS_INVALID_STATE;

	case ECONNRESET: return LS_INVALID_STATE;
	case ENOBUFS: return LS_OUT_OF_MEMORY;

	case ENOTCONN: return LS_INVALID_STATE;

	case ETIMEDOUT: return LS_TIMEDOUT;

	case EDQUOT: return LS_DISK_FULL;

	case EOWNERDEAD: return LS_INVALID_STATE;
	case ENOTRECOVERABLE: return LS_INVALID_STATE;
	}
}

int ls_access_to_oflags(int access)
{
	int oflags = 0;

	if (access & LS_FILE_READ)
	{
		if (access & LS_FILE_WRITE)
			oflags = O_RDWR;
		else
			oflags = O_RDONLY;
	}
	else if (access & LS_FILE_WRITE)
		oflags = O_WRONLY;

	return oflags;
}

int ls_create_to_oflags(int create)
{
	int oflags = 0;

	switch (create)
	{
	case LS_CREATE_NEW: oflags |= O_CREAT | O_EXCL; break;
	case LS_CREATE_ALWAYS: oflags |= O_CREAT | O_TRUNC; break;
	case LS_OPEN_EXISTING: break;
	case LS_OPEN_ALWAYS: oflags |= O_CREAT; break;
	case LS_TRUNCATE_EXISTING: oflags |= O_TRUNC; break;
	default: break;
	}

	return oflags;
}

#endif // LS_POSIX

native_flags_t ls_protect_to_flags(int protect)
{
#if LS_WINDOWS
	DWORD flProtect = 0;
	int read, write, exec, copy;

	read = !!(protect & LS_PROT_READ);
	write = !!(protect & LS_PROT_WRITE);
	exec = !!(protect & LS_PROT_EXEC);
	copy = !!(protect & LS_PROT_WRITECOPY);

	if (read)
	{
		if (write)
		{
			if (exec)
				return copy ? PAGE_EXECUTE_WRITECOPY : PAGE_EXECUTE_READWRITE;
			return copy ? PAGE_WRITECOPY : PAGE_READWRITE;
		}

		if (exec)
			return copy ? PAGE_EXECUTE_WRITECOPY : PAGE_EXECUTE_READ;
		return copy ? PAGE_WRITECOPY : PAGE_READONLY;		
	}

	if (write)
	{
		if (exec)
			return copy ? PAGE_EXECUTE_WRITECOPY :PAGE_EXECUTE_READWRITE;
		return copy ? PAGE_WRITECOPY : PAGE_READWRITE;
	}

	if (exec)
		return copy ? PAGE_EXECUTE_WRITECOPY : PAGE_EXECUTE_READ;
	return copy ? PAGE_WRITECOPY : PAGE_NOACCESS;
#else
	int prot = 0;

	if (protect & LS_PROT_READ)
		prot |= PROT_READ;

	if (protect & (LS_PROT_WRITE | LS_PROT_WRITECOPY))
		prot |= PROT_WRITE;

	if (protect & LS_PROT_EXEC)
		prot |= PROT_EXEC;

	return prot;
#endif // LS_WINDOWS
}

int ls_flags_to_protect(native_flags_t prot)
{
#if LS_WINDOWS
	int protect = 0;

	if (prot & PAGE_READONLY)
		protect |= LS_PROT_READ;

	if (prot & PAGE_READWRITE)
		protect |= LS_PROT_WRITE;

	if (prot & PAGE_WRITECOPY)
		protect |= LS_PROT_WRITECOPY;

	if (prot & PAGE_EXECUTE)
		protect |= LS_PROT_EXEC;

	return protect;
#else
	int protect = 0;

	if (prot & PROT_READ)
		protect |= LS_PROT_READ;

	if (prot & PROT_WRITE)
		protect |= LS_PROT_WRITE;

	if (prot & PROT_EXEC)
		protect |= LS_PROT_EXEC;

	return protect;
#endif // LS_WINDOWS
}

#if LS_DARWIN
int kr_to_error(kern_return_t kr)
{
	switch (kr)
	{
	default: return LS_UNKNOWN_ERROR;
	case KERN_SUCCESS: return 0;
	case KERN_INVALID_ADDRESS: return LS_INVALID_ARGUMENT;
	case KERN_PROTECTION_FAILURE: return LS_ACCESS_DENIED;
	case KERN_NO_SPACE: return LS_OUT_OF_MEMORY;
	case KERN_INVALID_ARGUMENT: return LS_INVALID_ARGUMENT;
	case KERN_FAILURE: return LS_UNKNOWN_ERROR;
	case KERN_RESOURCE_SHORTAGE: return LS_OUT_OF_MEMORY;
	case KERN_NOT_RECEIVER: return LS_INVALID_STATE;
	case KERN_NO_ACCESS: return LS_ACCESS_DENIED;
	case KERN_MEMORY_FAILURE: return LS_INVALID_STATE;
	case KERN_ALREADY_IN_SET: return LS_ALREADY_EXISTS;
	case KERN_NOT_IN_SET: return LS_NOT_FOUND;
	case KERN_ABORTED: return LS_CANCELED;
	case KERN_INVALID_NAME: return LS_INVALID_ARGUMENT;
	case KERN_INVALID_TASK: return LS_INVALID_HANDLE;
	case KERN_INVALID_RIGHT: return LS_INVALID_ARGUMENT;
	case KERN_INVALID_VALUE: return LS_INVALID_ARGUMENT;
	case KERN_UREFS_OVERFLOW: return LS_OUT_OF_MEMORY;
	case KERN_INVALID_CAPABILITY: return LS_INVALID_ARGUMENT;
	case KERN_RIGHT_EXISTS: return LS_ALREADY_EXISTS;
	case KERN_INVALID_HOST: return LS_INVALID_HANDLE;
	case KERN_MEMORY_PRESENT: return LS_ALREADY_EXISTS;
	case KERN_MEMORY_RESTART_COPY: return LS_INVALID_STATE;
	case KERN_INVALID_PROCESSOR_SET: return LS_INVALID_HANDLE;
	case KERN_POLICY_LIMIT: return LS_OUT_OF_MEMORY;
	case KERN_INVALID_OBJECT: return LS_INVALID_HANDLE;
	case KERN_ALREADY_WAITING: return LS_BUSY;
	case KERN_EXCEPTION_PROTECTED: return LS_ACCESS_DENIED;
	case KERN_INVALID_LEDGER: return LS_INVALID_HANDLE;
	case KERN_INVALID_MEMORY_CONTROL: return LS_INVALID_HANDLE;
	case KERN_INVALID_SECURITY: return LS_ACCESS_DENIED;
	case KERN_NOT_DEPRESSED: return LS_INVALID_STATE;
	case KERN_TERMINATED: return LS_INVALID_STATE;
	case KERN_LOCK_SET_DESTROYED: return LS_INVALID_STATE;
	case KERN_LOCK_UNSTABLE: return LS_INVALID_STATE;
	case KERN_LOCK_OWNED: return LS_BUSY;
	case KERN_LOCK_OWNED_SELF: return LS_DEADLOCK;
	case KERN_SEMAPHORE_DESTROYED: return LS_INVALID_STATE;
	case KERN_RPC_SERVER_TERMINATED: return LS_INVALID_STATE;
	case KERN_RPC_TERMINATE_ORPHAN: return LS_INVALID_STATE;
	case KERN_RPC_CONTINUE_ORPHAN: return LS_INVALID_STATE;
	case KERN_NOT_SUPPORTED: return LS_NOT_SUPPORTED;
	case KERN_NODE_DOWN: return LS_NOT_READY;
	case KERN_NOT_WAITING: return LS_INVALID_STATE;
	case KERN_OPERATION_TIMED_OUT: return LS_TIMEDOUT;
	case KERN_CODESIGN_ERROR: return LS_ACCESS_DENIED;
	case KERN_POLICY_STATIC: return LS_NOT_SUPPORTED;
	case KERN_INSUFFICIENT_BUFFER_SIZE: return LS_BUFFER_TOO_SMALL;
	case KERN_DENIED: return LS_ACCESS_DENIED;
	case KERN_MISSING_KC: return LS_NOT_FOUND;
	case KERN_INVALID_KC: return LS_INVALID_IMAGE;
	case KERN_NOT_FOUND: return LS_NOT_FOUND;
	case kIOReturnError: return LS_UNKNOWN_ERROR;
	case kIOReturnNoMemory: return LS_OUT_OF_MEMORY;
	case kIOReturnNoResources: return LS_OUT_OF_MEMORY;
	case kIOReturnIPCError: return LS_IO_ERROR;
	case kIOReturnNoDevice: return LS_NOT_FOUND;
	case kIOReturnNotPrivileged: return LS_ACCESS_DENIED;
	case kIOReturnBadArgument: return LS_INVALID_ARGUMENT;
	case kIOReturnLockedRead: return LS_SHARING_VIOLATION;
	case kIOReturnLockedWrite: return LS_SHARING_VIOLATION;
	case kIOReturnExclusiveAccess: return LS_SHARING_VIOLATION;
	case kIOReturnBadMessageID: return LS_IO_ERROR;
	case kIOReturnUnsupported: return LS_NOT_SUPPORTED;
	case kIOReturnVMError: return LS_INVALID_STATE;
	case kIOReturnInternalError: return LS_INVALID_STATE;
	case kIOReturnCannotLock: return LS_ACCESS_DENIED;
	case kIOReturnNotReadable: return LS_ACCESS_DENIED;
	case kIOReturnNotWritable: return LS_ACCESS_DENIED;
	case kIOReturnBadMedia: return LS_INVALID_STATE;
	case kIOReturnStillOpen: return LS_INVALID_STATE;
	case kIOReturnRLDError: return LS_INVALID_STATE;
	case kIOReturnBusy: return LS_BUSY;
	case kIOReturnTimeout: return LS_TIMEDOUT;
	case kIOReturnOffline: return LS_NOT_READY;
	case kIOReturnNotAttached: return LS_NOT_READY;
	case kIOReturnNoChannels: return LS_NOT_READY;
	case kIOReturnNoSpace: return LS_DISK_FULL;
	case kIOReturnPortExists: return LS_ALREADY_EXISTS;
	case kIOReturnCannotWire: return LS_ACCESS_DENIED;
	case kIOReturnNoInterrupt: return LS_INVALID_STATE;
	case kIOReturnNoFrames: return LS_NO_DATA;
	case kIOReturnMessageTooLarge: return LS_BUFFER_TOO_SMALL;
	case kIOReturnNotPermitted: return LS_ACCESS_DENIED;
	case kIOReturnNoPower: return LS_NOT_READY;
	case kIOReturnNoMedia: return LS_NOT_READY;
	case kIOReturnUnformattedMedia: return LS_INVALID_STATE;
	case kIOReturnUnsupportedMode: return LS_NOT_SUPPORTED;
	case kIOReturnUnderrun: return LS_IO_ERROR;
	case kIOReturnOverrun: return LS_IO_ERROR;
	case kIOReturnDeviceError: return LS_IO_ERROR;
	case kIOReturnNoCompletion: return LS_INVALID_STATE;
	case kIOReturnAborted: return LS_CANCELED;
	case kIOReturnNoBandwidth: return LS_OUT_OF_MEMORY;
	case kIOReturnIsoTooOld: return LS_INVALID_STATE;
	case kIOReturnIsoTooNew: return LS_INVALID_STATE;
	case kIOReturnNotFound: return LS_NOT_FOUND;
	case kIOReturnInvalid: return LS_INVALID_ARGUMENT;
	}
}
#endif // LS_DARWIN

LS_THREADLOCAL int _ls_errno = 0;
