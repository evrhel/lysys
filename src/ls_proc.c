#include <lysys/ls_proc.h>

#include <lysys/ls_core.h>
#include <lysys/ls_string.h>
#include <lysys/ls_shell.h>
#include <lysys/ls_file.h>

#include "ls_native.h"
#include "ls_handle.h"
#include "ls_buffer.h"
#include "ls_util.h"
#include "ls_file_priv.h"

#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>

#if LS_DARWIN
#include <libproc.h>
#endif // LS_DARWIN

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

static size_t salen(const char *const *arr)
{
	size_t n = 0;
	for (; *arr; arr++) n++;
	return n;
}

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
	ls_file_t *pf;
	int flags;

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

	ph = ls_handle_create(&ProcClass, 0);
	if (!ph)
		goto generic_error;

	ph->si.cb = sizeof(STARTUPINFOW);

	if (info)
	{
		if (info->hstdin || info->hstdout || info->hstderr)
		{
			if (info->hstdin && info->hstdin != LS_STDIN)
			{
				pf = ls_resolve_file(info->hstdin, &flags);
				if (!pf)
					goto generic_error;

				if (flags & LS_FLAG_ASYNC)
				{
					ls_set_errno(LS_INVALID_HANDLE);
					goto generic_error;
				}

				ph->si.hStdInput = pf->hFile;
				if (!SetHandleInformation(ph->si.hStdInput, HANDLE_FLAG_INHERIT, 1))
					goto generic_error;
			}

			if (info->hstdout && info->hstdout != LS_STDOUT)
			{
				pf = ls_resolve_file(info->hstdin, &flags);
				if (!pf)
					goto generic_error;

				if (flags & LS_FLAG_ASYNC)
				{
					ls_set_errno(LS_INVALID_HANDLE);
					goto generic_error;
				}

				ph->si.hStdOutput = pf->hFile;
				if (!SetHandleInformation(ph->si.hStdOutput, HANDLE_FLAG_INHERIT, 1))
					goto generic_error;
			}

			if (info->hstderr && info->hstderr != LS_STDERR)
			{
				pf = ls_resolve_file(info->hstdin, &flags);
				if (!pf)
					goto generic_error;

				if (flags & LS_FLAG_ASYNC)
				{
					ls_set_errno(LS_INVALID_HANDLE);
					goto generic_error;
				}

				ph->si.hStdError = pf->hFile;
				if (!SetHandleInformation(ph->si.hStdError, HANDLE_FLAG_INHERIT, 1))
					goto generic_error;
			}

			ph->si.dwFlags |= STARTF_USESTDHANDLES;
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
	struct ls_proc *ph;
	int fd[2] = { 0, 0 }; // pipe (read, write)
	int rc;
	int err;
	ssize_t bytes_read;
	struct ls_file *pf;
	int flags;

	char **argv2 = NULL;
	size_t argv_len = 0;

	char **envp = NULL;
	size_t env_len = 0;
	int stdin_fd = STDIN_FILENO, stdout_fd = STDOUT_FILENO, stderr_fd = STDERR_FILENO;
	const char *cwd = NULL;
	ssize_t i;

	if (!path || !argv)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	ph = ls_handle_create(&ProcClass, 0);
	if (!ph)
		return NULL;

	if (argv)
	{
		argv_len = salen(argv);
		argv2 = ls_calloc((argv_len + 1), sizeof(char *));
		if (!argv2)
			goto create_error;

		for (i = 0; i < argv_len; i++)
		{
			argv2[i] = ls_strdup(argv[i]);
			if (!argv2[i])
				goto create_error;
		}
	}

	if (info)
	{
		if (info->cwd)
			cwd = info->cwd;

		if (envp)
		{
			env_len = salen(info->envp);
			if (env_len > 0)
			{
				envp = ls_calloc((env_len + 1), sizeof(char *));
				if (!envp)
					goto create_error;

				for (i = 0; i < env_len; i++)
				{
					envp[i] = ls_strdup(info->envp[i]);
					if (!envp[i])
						goto create_error;
				}
			}
		}

		if (info->hstdin)
		{
			pf = ls_resolve_file(info->hstdin, &flags);
			if (!pf)
			{
				err = _ls_errno;
				goto create_error;
			}

			stdin_fd = pf->fd;
		}

		if (info->hstdout)
		{
			pf = ls_resolve_file(info->hstdout, &flags);
			if (!pf)
			{
				err = _ls_errno;
				goto create_error;
			}

			stdout_fd = pf->fd;
		}

		if (info->hstderr)
		{
			pf = ls_resolve_file(info->hstderr, &flags);
			if (!pf)
			{
				err = _ls_errno;
				goto create_error;
			}

			stderr_fd = pf->fd;
		}
	}

	rc = pipe(fd);
	if (rc == -1)
	{
		err = ls_errno_to_error(errno);
		goto create_error;
	}

	ph->pid = fork();
	if (ph->pid == -1)
	{
		err = ls_errno_to_error(errno);
		goto create_error;
	}

	if (ph->pid == 0)
	{
		// child

		close(fd[0]); // close read end

		// set close on exec flag for write end
		rc = fcntl(fd[1], F_SETFD, FD_CLOEXEC);
		if (rc == -1)
		{
			err = ls_errno_to_error(errno);
			goto exec_error;
		}

		if (cwd)
		{
			// change working directory
			rc = chdir(cwd);
			if (rc == -1)
			{
				err = ls_errno_to_error(errno);
				goto exec_error;
			}
		}

		if (stdin_fd)
		{
			// redirect stdin
			rc = dup2(stdin_fd, STDIN_FILENO);
			if (rc == -1)
			{
				err = ls_errno_to_error(errno);
				goto exec_error;
			}
		}

		if (stdout_fd)
		{
			// redirect stdout
			rc = dup2(stdout_fd, STDOUT_FILENO);
			if (rc == -1)
			{
				err = ls_errno_to_error(errno);
				goto exec_error;
			}
		}

		if (stderr_fd)
		{
			// redirect stderr
			rc = dup2(stderr_fd, STDERR_FILENO);
			if (rc == -1)
			{
				err = ls_errno_to_error(errno);
				goto exec_error;
			}
		}

		execve(path, argv2, envp);
		err = ls_errno_to_error(errno);

		// fall through
	exec_error:
		(void)write(fd[1], &err, sizeof(err));
		fsync(fd[1]);
		close(fd[1]);
		_exit(1);
	}

	// parent
	
	for (i = 0; i < env_len; i++)
		ls_free(envp[i]);
	ls_free(envp);
	envp = NULL, env_len = 0;

	for (i = 0; i < argv_len; i++)
		ls_free(argv2[i]);
	ls_free(argv2);
	argv2 = NULL, argv_len = 0;

	close(fd[1]), fd[1] = 0; // close write end

	bytes_read = read(fd[0], &err, sizeof(err));
	if (bytes_read == -1)
	{
		// unknown error
		err = ls_errno_to_error(errno);
		goto create_error;
	}

	if (bytes_read == 4)
	{
		// an error occurred in the parent
		waitpid(ph->pid, NULL, 0);
		goto create_error;
	}

	return ph;
create_error:
	for (i = 0; i < env_len; i++)
		ls_free(envp[i]);
	ls_free(envp);

	for (i = 0; i < argv_len; i++)
		ls_free(argv2[i]);
	ls_free(argv2);

	if (fd[0])
		close(fd[0]);

	if (fd[1])
		close(fd[1]);

	ls_handle_dealloc(ph);
	return NULL;
#endif // LS_WINDOWS
}

ls_handle ls_proc_start_shell(const char *path, const char *argv[], const struct ls_proc_start_info *info)
{
#if LS_WINDOWS
	char cmd[MAX_PATH];
	size_t len;
	const char *new_argv[3];
	ls_handle ph;
	LPWSTR lpCmd;

	if (!path)
	{
		ls_set_errno(LS_INVALID_ARGUMENT);
		return NULL;
	}

	len = ls_getenv_buf("COMSPEC", cmd, sizeof(cmd));
	if (len == -1)
	{
		ls_set_errno(LS_NOT_FOUND);
		return NULL;
	}

	lpCmd = ls_build_command_line(path, argv);
	if (!lpCmd)
		return NULL;

	new_argv[0] = "/C";

	new_argv[1] = ls_wchar_to_utf8(lpCmd);
	if (!new_argv[1])
	{
		ls_free(lpCmd);
		return NULL;
	}

	new_argv[2] = NULL;

	ph = ls_proc_start(cmd, new_argv, info);

	ls_free((char *)new_argv[1]);
	ls_free(lpCmd);

	return ph;
#else
	ls_set_errno(LS_NOT_IMPLEMENTED);
	return NULL;
#endif // LS_WINDOWS
}

int ls_proc_start_wait(const char *path, const char *argv[], const struct ls_proc_start_info *info)
{
	ls_handle ph;
	int rc;
	int exit_code;

	_ls_errno = 0;

	ph = ls_proc_start(path, argv, info);
	if (!ph)
		return -1;

	rc = ls_wait(ph);
	if (rc == -1)
	{
		rc = _ls_errno;
		ls_close(ph);
		return ls_set_errno(rc);
	}

	rc = ls_proc_exit_code(ph, &exit_code);
	if (rc == -1)
		exit_code = -1;

	ls_close(ph);

	return exit_code;
}

int ls_proc_start_shell_wait(const char *path, const char *argv[], const struct ls_proc_start_info *info)
{
	ls_handle ph;
	int rc;
	int exit_code;

	_ls_errno = 0;

	ph = ls_proc_start_shell(path, argv, info);
	if (!ph)
		return -1;

	rc = ls_wait(ph);
	if (rc == -1)
	{
		rc = _ls_errno;
		ls_close(ph);
		return ls_set_errno(rc);
	}

	rc = ls_proc_exit_code(ph, &exit_code);
	if (rc == -1)
		exit_code = -1;

	ls_close(ph);

	return exit_code;
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

	ph = ls_handle_create(&ProcClass, 0);
	if (!ph)
	{
		CloseHandle(hProcess);
		return NULL;
	}

	ph->pi.hProcess = hProcess;
	ph->pi.dwProcessId = pid;

	ph->path_len = ls_image_name(hProcess, ph->path, sizeof(ph->path), &ph->name);

	return ph;
#elif LS_DARWIN
    struct ls_proc *proc;
    int rc;
    char buf[PROC_PIDPATHINFO_MAXSIZE];
    
    proc = ls_handle_create(&ProcClass, 0);
    if (!proc)
        return NULL;
    
    rc = proc_pidpath((int)pid, buf, sizeof(buf));
    if (rc <= 0)
    {
        ls_set_errno_errno(errno);
        ls_handle_dealloc(proc);
        return NULL;
    }
    
    proc->path_len = strlen(buf);
    proc->path = ls_malloc(proc->path_len + 1);
    if (!proc->path)
    {
        ls_handle_dealloc(proc);
        return NULL;
    }
    
    memcpy(proc->path, buf, proc->path_len);
    proc->path[proc->path_len] = 0; // null terminate
    
    proc->name = strrchr(proc->path, '/');
    if (proc->name)
        proc->name++;
    
    return proc;
#else
	struct ls_proc *proc;
	struct stat st;
	char filename[64];
	int fd;
	int rc;
	ssize_t n;

	if (getpid() == pid)
		return LS_SELF;

	proc = ls_handle_create(&ProcClass, 0);
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

	if (ph == LS_SELF)
		ExitProcess(exit_code);

	if (ls_type_check(ph, LS_PROC))
		return -1;

	b = TerminateProcess(proc->pi.hProcess, exit_code);
	if (!b)
		return ls_set_errno_win32(GetLastError());

	return 0;
#else
	struct ls_proc *proc = ph;
	int rc;

	if (ph == LS_SELF)
		exit(exit_code);

	if (ls_type_check(ph, LS_PROC))
		return -1;

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

	if (ph == LS_SELF)
		return LS_PROC_RUNNING;

	if (ls_type_check(ph, LS_PROC))
		return -1;

	dwResult = WaitForSingleObject(proc->pi.hProcess, 0);

	if (dwResult == WAIT_TIMEOUT)
		return LS_PROC_RUNNING;

	if (dwResult == WAIT_OBJECT_0)
		return LS_PROC_TERMINATED;

	return ls_set_errno_win32(GetLastError());
#else
	struct ls_proc *proc = ph;
	int rc;

	if (proc->status != LS_PROC_RUNNING)
		return proc->status;

	if (ls_type_check(ph, LS_PROC))
		return -1;

	rc = kill(proc->pid, 0);
	if (rc == 0)
		return LS_PROC_RUNNING;

	if (errno == ESRCH)
	{
		proc->status = LS_PROC_TERMINATED;
		return LS_PROC_TERMINATED;
	}

	return ls_set_errno(ls_errno_to_error(errno));
#endif // LS_WINDOWS
}

int ls_proc_exit_code(ls_handle ph, int *exit_code)
{
#if LS_WINDOWS
	DWORD dwExitCode;
	DWORD dwResult;
	struct ls_proc *proc = ph;

	if (ph == LS_SELF)
		return 1;

	if (ls_type_check(ph, LS_PROC))
		return -1;

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

	if (ph == LS_SELF)
		return 1;

	if (ls_type_check(ph, LS_PROC))
		return -1;

	if (proc->status == 0)
		return 1;

	if (exit_code)
		*exit_code = WEXITSTATUS(proc->status);

	return 0;
#endif // LS_WINDOWS
}

static size_t ls_get_self_path(char *path, size_t size)
{
#if LS_WINDOWS
	return ls_set_errno(LS_NOT_IMPLEMENTED);
#elif LS_DARWIN
    int rc;
    
    rc = proc_pidpath(getpid(), path, (uint32_t)size);
    if (rc == -1)
        return ls_set_errno(LS_UNKNOWN_ERROR);
    return rc;
#else
    return ls_set_errno(LS_NOT_IMPLEMENTED);
#endif // LS_WINDOWS
}

static size_t ls_proc_self_name(char *name, size_t size)
{
#if LS_WINDOWS
    return ls_set_errno(LS_NOT_IMPLEMENTED);
#elif LS_DARWIN
    int rc;
    
    rc = proc_name(getpid(), name, (uint32_t)size);
    if (rc == -1)
        return ls_set_errno(LS_UNKNOWN_ERROR);
    return rc;
#else
    return ls_set_errno(LS_NOT_IMPLEMENTED);
#endif // LS_WINDOWS
}

size_t ls_proc_path(ls_handle ph, char *path, size_t size)
{
	struct ls_proc *proc = ph;

	if (!path != !size)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	if (ph == LS_SELF)
		return ls_get_self_path(path, size);

	if (ls_type_check(ph, LS_PROC))
		return -1;

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

	if (!name != !size)
		return ls_set_errno(LS_INVALID_ARGUMENT);

	if (ph == LS_SELF)
		return ls_proc_self_name(name, size);

	if (ls_type_check(ph, LS_PROC))
		return -1;

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

	if (ph == LS_SELF)
		return GetCurrentProcessId();

	if (ls_type_check(ph, LS_PROC))
		return -1;

	return proc->pi.dwProcessId;
#else
	struct ls_proc *proc = ph;

	if (ph == LS_SELF)
		return getpid();

	if (ls_type_check(ph, LS_PROC))
		return -1;

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
