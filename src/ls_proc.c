#include <lysys/ls_proc.h>

#include <lysys/ls_core.h>

#include "ls_native.h"
#include "ls_handle.h"
#include "ls_buffer.h"
#include "ls_util.h"

#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>

struct ls_proc
{
#if LS_WINDOWS
	STARTUPINFOW si;
	PROCESS_INFORMATION pi;

	char path[MAX_PATH];
	char *name;
	size_t path_len; // excludes null terminator
#else
	pid_t pid;
	int status;
	
	char *path;
	char *name;
	size_t path_len;
#endif // LS_WINDOWS
};

#if LS_POSIX
static void alarm_handler(int sig) { (void)sig; }

static void *wait_handler(void *param)
{
	pid_t pid = (pid_t)(intptr_t)param;
	int rc;

retry:
	rc = waitpid(pid, NULL, 0);
	if (rc == -1)
	{
		if (errno == EINTR)
			goto retry;
	}

	return NULL;
}

#endif // LS_POSIX

static void ls_proc_dtor(struct ls_proc *proc)
{
#if LS_WINDOWS
	if (proc->pi.hProcess)
		CloseHandle(proc->pi.hProcess);	

	if (proc->pi.hThread)
		CloseHandle(proc->pi.hThread);
#else
	int rc;
	pthread_t pt;
	intptr_t ipid;
	pid_t pid;
	
	if (proc->path)
		ls_free(proc->path);
	
	pid = waitpid(proc->pid, NULL, WNOHANG);
	if (pid == 0)
	{
		// not exited, reap child on daemon thread
		ipid = proc->pid;
		rc = pthread_create(&pt, NULL, &wait_handler, (void *)ipid);
		if (rc == 0)
			(void)pthread_detach(pt);
		else
			(void)waitpid(proc->pid, NULL, 0); // blocking
	}
#endif // LS_WINDOWS
}

static int ls_proc_wait(struct ls_proc *proc, unsigned long ms)
{
#if LS_WINDOWS
	DWORD dwResult;

	dwResult = WaitForSingleObject(proc->pi.hProcess, ms);
	if (dwResult == WAIT_OBJECT_0)
		return 0;

	if (dwResult == WAIT_TIMEOUT)
		return 1;

	return ls_set_errno_win32(GetLastError());
#else
	int rc;
	int status;
	useconds_t useconds;
	struct sigaction sa;
	
	if (proc->status)
		return 0;
	
	if (ms == LS_INFINITE)
	{
		rc = waitpid(proc->pid, &status, 0);
		if (rc == -1)
			return ls_set_errno(ls_errno_to_error(errno));
	}
	else
	{
		sa.sa_handler = &alarm_handler;
		(void)sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		rc = sigaction(SIGALRM, &sa, NULL);
		if (rc == -1)
			return ls_set_errno(ls_errno_to_error(errno));
		
		useconds = (useconds_t)(ms * 1000);
		rc = ualarm(useconds, 0);
		if (rc == -1)
			return ls_set_errno(ls_errno_to_error(errno));
		
		rc = waitpid(proc->pid, &status, 0);
		if (rc == -1)
		{
			if (errno == EINTR)
				return 1;

			ls_set_errno(ls_errno_to_error(errno));

			(void)ualarm(0, 0); // disable interrupt
			return -1;
		}
		
		(void)ualarm(0, 0); // disable interrupt
	}
	
	proc->status = status;
	
	return 0;
#endif // LS_WINDOWS
}

static const struct ls_class ProcClass = {
	.type = LS_PROC,
	.cb = sizeof(struct ls_proc),
	.dtor = (ls_dtor_t)&ls_proc_dtor,
	.wait = (ls_wait_t)&ls_proc_wait
};

#if LS_WINDOWS
static size_t ls_image_name(HANDLE hProcess, char *out, size_t size, char **name)
{
	WCHAR path[MAX_PATH];
	DWORD dwCount = MAX_PATH;
	BOOL bRet;
	size_t len;
	char *p;

	bRet = QueryFullProcessImageNameW(hProcess, 0, path, &dwCount);
	if (!bRet)
	{
		*name = NULL;
		return 0;
	}

	len = ls_wchar_to_utf8_buf(path, out, size);
	if (len == -1)
	{
		*name = NULL;
		return 0;
	}

	p = strrchr(out, '\\');
	if (p) *name = p + 1;
	else *name = out;

	return len;
}
#endif

#if LS_POSIX
static size_t salen(const char *const *arr)
{
	size_t n = 0;
	for (; *arr; arr++) n++;
	return n;
}
#endif // LS_POSIX

ls_handle ls_proc_start(const char *path, const char *argv[], const struct ls_proc_start_info *info)
{
#if LS_WINDOWS
	struct ls_proc *ph = NULL;
	BOOL b;
	LPWSTR lpCmd = NULL;
	LPWSTR lpEnv = NULL;
	LPWSTR lpDir = NULL;
	DWORD dwErr;
	DWORD dwFlags = 0; 
	int err;

	lpCmd = ls_build_command_line(path, argv);
	if (!lpCmd)
		return NULL;

	if (info)
	{
		if (info->envp)
		{
			lpEnv = ls_build_environment(info->envp);
			if (!lpEnv)
				goto generic_error;
		}

		if (info->cwd)
		{
			lpDir = ls_utf8_to_wchar(info->cwd);
			if (!lpDir)
				goto generic_error;
		}
	}

	ph = ls_handle_create(&ProcClass);
	if (!ph)
		goto generic_error;

	ph->si.cb = sizeof(STARTUPINFOW);

	if (info)
	{
		if (info->hstdin || info->hstdout || info->hstderr)
		{
			dwFlags |= STARTF_USESTDHANDLES;

			ph->si.hStdInput = ls_resolve_file(info->hstdin);
			if (!ph->si.hStdInput)
				goto generic_error;

			ph->si.hStdOutput = ls_resolve_file(info->hstdout);
			if (!ph->si.hStdOutput)
				goto generic_error;

			ph->si.hStdError = ls_resolve_file(info->hstderr);
			if (!ph->si.hStdError)
				goto generic_error;
		}
	}

	b = CreateProcessW(NULL, lpCmd, NULL, NULL, TRUE, dwFlags, lpEnv, lpDir, &ph->si, &ph->pi);
	if (!b)
	{
		dwErr = GetLastError();

		ls_handle_dealloc(ph);
		ls_free(lpDir);
		ls_free(lpEnv);
		ls_free(lpCmd);

		ls_set_errno_win32(dwErr);
		return NULL;
	}

	ls_free(lpDir);
	ls_free(lpEnv);
	ls_free(lpCmd);

	ph->path_len = ls_image_name(ph->pi.hProcess, ph->path, sizeof(ph->path), &ph->name);

	return ph;

generic_error:
	err = _ls_errno;
	ls_handle_dealloc(ph);
	ls_free(lpDir);
	ls_free(lpEnv);
	ls_free(lpCmd);
	ls_set_errno(err);
	return NULL;
#else
	ls_set_errno(LS_NOT_IMPLEMENTED);
	return NULL;
#endif // LS_WINDOWS
}

ls_handle ls_proc_open(unsigned long pid)
{
#if LS_WINDOWS
	struct ls_proc *ph;
	HANDLE hProcess;

	if (GetCurrentProcessId() == pid)
		return LS_SELF;

	hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
	if (!hProcess)
	{
		ls_set_errno_win32(GetLastError());
		return NULL;
	}

	ph = ls_handle_create(&ProcClass);
	if (!ph)
	{
		CloseHandle(hProcess);
		return NULL;
	}

	ph->pi.hProcess = hProcess;
	ph->pi.dwProcessId = pid;

	ph->path_len = ls_image_name(hProcess, ph->path, sizeof(ph->path), &ph->name);

	return ph;
#else
	struct ls_proc *proc;
	struct stat st;
	char filename[64];
	int fd;
	int rc;
	ssize_t n;

	if (getpid() == pid)
		return LS_SELF;
	
	proc = ls_handle_create(&ProcClass);
	if (!proc)
		return NULL;
	
	(void)snprintf(filename, sizeof(filename), "/proc/%lu/exe", pid);
	
	rc = stat(filename, &st);
	if (rc == -1)
	{
		ls_set_errno(ls_errno_to_error(errno));
		ls_handle_dealloc(proc);
		return NULL;
	}
	
	proc->path_len = st.st_size;
	proc->path = ls_malloc(proc->path_len + 1);
	if (!proc->path)
	{
		ls_handle_dealloc(proc);
		return NULL;
	}
	
	fd = open(filename, O_RDONLY);
	if (fd == -1)
	{
		ls_set_errno(ls_errno_to_error(errno));
		ls_free(proc->path);
		ls_handle_dealloc(proc);
		return NULL;
	}
	
	n = read(fd, proc->path, proc->path_len);
	if (n == -1)
	{
		ls_set_errno(ls_errno_to_error(errno));
		ls_free(proc->path);
		ls_handle_dealloc(proc);
		return NULL;
	}
	
	proc->path[n] = 0;
	
	(void)close(fd);
	
	proc->name = strrchr(proc->path, '/');
	if (!proc->name)
		proc->name = proc->path;
	else
		proc->name++;
	
	proc->pid = (pid_t)pid;
	
	return proc;
#endif // LS_WINDOWS
}

int ls_kill(ls_handle ph, int exit_code)
{
#if LS_WINDOWS
	BOOL b;
	struct ls_proc *proc = ph;

	if (!ph)
		return ls_set_errno(LS_INVALID_HANDLE);

	if (ph == LS_SELF)
		ExitProcess(exit_code);

	b = TerminateProcess(proc->pi.hProcess, exit_code);
	if (!b)
		return ls_set_errno_win32(GetLastError());

	return 0;
#else
	struct ls_proc *proc = ph;
	int rc;

	if (!ph)
		return ls_set_errno(LS_INVALID_HANDLE);

	if (ph == LS_SELF)
		exit(exit_code);

	rc = kill(proc->pid, 1);
	if (rc == -1)
		return ls_set_errno(ls_errno_to_error(errno));
	return 0;
#endif // LS_WINDOWS
}

int ls_proc_state(ls_handle ph)
{
#if LS_WINDOWS
	DWORD dwResult;
	struct ls_proc *proc = ph;

	if (!ph)
		return -1;

	if (ph == LS_SELF)
		return LS_PROC_RUNNING;

	dwResult = WaitForSingleObject(proc->pi.hProcess, 0);

	if (dwResult == WAIT_TIMEOUT)
		return LS_PROC_RUNNING;

	if (dwResult == WAIT_OBJECT_0)
		return LS_PROC_TERMINATED;

	return ls_set_errno_win32(GetLastError());
#else    
	return ls_set_errno(LS_NOT_IMPLEMENTED);
#endif // LS_WINDOWS
}

int ls_proc_exit_code(ls_handle ph, int *exit_code)
{
#if LS_WINDOWS
	DWORD dwExitCode;
	DWORD dwResult;
	struct ls_proc *proc = ph;

	if (!ph)
		return ls_set_errno(LS_INVALID_HANDLE);

	if (ph == LS_SELF)
		return 1;

	dwResult = WaitForSingleObject(proc->pi.hProcess, 0);
	if (dwResult == WAIT_TIMEOUT)
		return 1;

	if (dwResult == WAIT_OBJECT_0)
	{
		if (!GetExitCodeProcess(proc->pi.hProcess, &dwExitCode))
			return ls_set_errno_win32(GetLastError());

		if (exit_code)
			*exit_code = dwExitCode;

		return 0;
	}

	return ls_set_errno_win32(GetLastError());
#else
	struct ls_proc *proc = ph;

	if (!ph)
		return ls_set_errno(LS_INVALID_HANDLE);

	if (ph == LS_SELF)
		return 1;
	
	if (proc->status == 0)
		return 1;
	
	if (exit_code)
		*exit_code = WEXITSTATUS(proc->status);

	return 0;
#endif // LS_WINDOWS
}

static size_t ls_get_self_path(char *path, size_t size)
{
	return ls_set_errno(LS_NOT_IMPLEMENTED);
}

static size_t ls_proc_self_name(char *name, size_t size)
{
	return ls_set_errno(LS_NOT_IMPLEMENTED);
}

size_t ls_proc_path(ls_handle ph, char *path, size_t size)
{
	struct ls_proc *proc = ph;

	if (!ph)
		return ls_set_errno(LS_INVALID_HANDLE);

	if (!path != !size)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	if (ph == LS_SELF)
		return ls_get_self_path(path, size);
	
	if (size == 0)
		return proc->path_len;

	if (size < proc->path_len)
		return ls_set_errno(LS_BUFFER_TOO_SMALL);

	memcpy(path, proc->path, proc->path_len);
	return proc->path_len - 1;
}

size_t ls_proc_name(ls_handle ph, char *name, size_t size)
{
	struct ls_proc *proc = ph;
	size_t len;

	if (!ph)
		return ls_set_errno(LS_INVALID_HANDLE);

	if (!name != !size)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	if (ph == LS_SELF)
		return ls_proc_self_name(name, size);

	len = proc->path_len - (proc->name - proc->path);

	if (size == 0)
		return len;

	if (size < len)
		return ls_set_errno(LS_BUFFER_TOO_SMALL);

	memcpy(name, proc->name, len);
	return len - 1;
}

unsigned long ls_getpid(ls_handle ph)
{
#if LS_WINDOWS
	struct ls_proc *proc = ph;

	if (!ph)
		return ls_set_errno(LS_INVALID_HANDLE);

	if (ph == LS_SELF)
		return GetCurrentProcessId();

	return proc->pi.dwProcessId;
#else
	struct ls_proc *proc = ph;

	if (!ph)
		return ls_set_errno(LS_INVALID_HANDLE);

	if (ph == LS_SELF)
		return getpid();

	return proc->pid;
#endif // LS_WINDOWS
}

unsigned long ls_getpid_self(void)
{
#if LS_WINDOWS
	return GetCurrentProcessId();
#else
	return getpid();
#endif // LS_WINDOWS
}

ls_handle ls_proc_self(void) { return LS_SELF; }
