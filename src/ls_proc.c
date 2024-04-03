#include <lysys/ls_proc.h>

#include <lysys/ls_core.h>

#include "ls_native.h"
#include "ls_handle.h"
#include "ls_buffer.h"

#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <signal.h>

#define LS_PROC_SELF ((ls_handle *)-1)
#define LS_PIPE_NAME_SIZE 256
#define LS_PIPE_BUF_SIZE 4096

struct ls_proc
{
#if LS_WINDOWS
	STARTUPINFOW si;
	PROCESS_INFORMATION pi;

	char path[MAX_PATH];
	char *name;
	int path_len;
#else
    pid_t pid;
    int status;
#endif // LS_WINDOWS
};

#if LS_POSIX
static void alarm_handler(int sig) { (void)sig; }

static void *wait_handler(void *param)
{
    intptr_t iptr = param;
    pid_t pid = (pid_t)iptr;
    
    waitpid(pid, NULL, 0);
    
    return NULL;
}

#endif

static void LS_CLASS_FN ls_proc_dtor(struct ls_proc *proc)
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
    
    pid = waitpid(proc->pid, NULL, WNOHANG);
    if (pid == 0)
    {
        // not exited, reap child on daemon thread
        ipid = proc->pid;
        rc = pthread_create(&pt, NULL, &wait_handler, ipid);
        if (rc == 0)
            pthread_detach(pt);
    }
#endif // LS_WINDOWS
}

static int LS_CLASS_FN ls_proc_wait(struct ls_proc *proc, unsigned long ms)
{
#if LS_WINDOWS
	DWORD dwResult;

	if (!proc->pi.hProcess) return 0;

	dwResult = WaitForSingleObject(proc->pi.hProcess, ms);
	if (dwResult == WAIT_OBJECT_0)
		return 0;
	return -1;
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
            return -1;
    }
    else
    {
        sa.sa_handler = &alarm_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        rc = sigaction(SIGALRM, &sa, NULL);
        if (rc == -1)
            return -1;
        
        useconds = (useconds_t)(ms * 1000);
        ualarm(useconds, 0);
        
        rc = waitpid(proc->pid, &status, 0);
        if (rc == -1)
        {
            if (errno == EINTR)
                return 1;
            return -1;
        }
        
        ualarm(0, 0); // disable interrupt
    }
    
    proc->status = status;
    
    return 0;
#endif // LS_WINDOWS
}

static struct ls_class ProcClass = {
	.type = LS_PROC,
	.cb = sizeof(struct ls_proc),
	.dtor = (ls_dtor_t)&ls_proc_dtor,
	.wait = (ls_wait_t)&ls_proc_wait
};

#if LS_WINDOWS

struct ls_pipe_server_win32
{
	HANDLE hPipe;
	OVERLAPPED ov;

	WCHAR szPipeName[LS_PIPE_NAME_SIZE];
};

struct ls_pipe_client_thread
{
	HANDLE hThread;
	int was_closed;
	WCHAR szPipeName[LS_PIPE_NAME_SIZE];

	CRITICAL_SECTION cs;

	int *is_error;
	PHANDLE phPipe;
};

struct ls_pipe_client_win32
{
	HANDLE hPipe;
	struct ls_pipe_client_thread *thread;
	int is_error;
	WCHAR szPipeName[LS_PIPE_NAME_SIZE];
};

static void LS_CLASS_FN ls_pipe_server_dtor(struct ls_pipe_server_win32 *pipe)
{
	if (pipe->hPipe)
		CloseHandle(pipe->hPipe);

	if (pipe->ov.hEvent)
		CloseHandle(pipe->ov.hEvent);
}

static int LS_CLASS_FN ls_pipe_server_wait(struct ls_pipe_server_win32 *pipe, unsigned long ms)
{
	DWORD dwResult;

	if (!pipe->hPipe) return 0;

	dwResult = WaitForSingleObject(pipe->ov.hEvent, ms);
	if (dwResult == WAIT_OBJECT_0)
		return 0;
	return -1;
}

#else

static void LS_CLASS_FN ls_pipe_server_dtor(void *ignored)
{
    // TODO: implement
}

static int LS_CLASS_FN ls_pipe_server_wait(void *ignored, unsigned long ms)
{
    // TODO: implement
    return 0;
}

#endif // LS_WINDOWS

#if LS_WINDOWS

static int ls_try_open_pipe(LPCWSTR szPipeName, PHANDLE phPipe)
{
	*phPipe = CreateFileW(szPipeName, GENERIC_READ | GENERIC_WRITE,
		0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

	if (*phPipe != INVALID_HANDLE_VALUE)
		return 0;

	if (GetLastError() == ERROR_PIPE_BUSY)
		return 1;

	return -1;
}

static int ls_pipe_open_now(LPCWSTR szPipeName, PHANDLE phPipe, unsigned long ms)
{
	int rc;
	BOOL bRet;

	rc = ls_try_open_pipe(szPipeName, phPipe);
	if (rc == 0) return 0;
	if (rc == -1) return -1;

	bRet = WaitNamedPipeW(szPipeName, ms);
	if (!bRet) return -1;

	return ls_try_open_pipe(szPipeName, phPipe);
}

static DWORD CALLBACK ls_pipe_client_wait_thread(LPVOID lpParam)
{
	struct ls_pipe_client_thread *thread = lpParam;
	int is_error = 0;
	int rc;
	BOOL bRet;
	HANDLE hPipe = NULL;

	for (;;)
	{
		EnterCriticalSection(&thread->cs);
		if (thread->was_closed)
		{
			LeaveCriticalSection(&thread->cs);
			break;
		}
		LeaveCriticalSection(&thread->cs);

		rc = ls_try_open_pipe(thread->szPipeName, &hPipe);
		if (rc == 0) break;
		
		if (rc == -1)
		{
			is_error = 1;
			break;
		}

		bRet = WaitNamedPipeW(thread->szPipeName, NMPWAIT_USE_DEFAULT_WAIT);
		if (bRet == -1)
		{
			is_error = 1;
			break;
		}
	}

	EnterCriticalSection(&thread->cs);

	if (!thread->was_closed)
	{
		*thread->phPipe = hPipe;
		*thread->is_error = is_error;
	}
	else if (hPipe)
		CloseHandle(hPipe);

	LeaveCriticalSection(&thread->cs);

	DeleteCriticalSection(&thread->cs);
	CloseHandle(thread->hThread);
	ls_free(thread);

	return 0;
}

static void LS_CLASS_FN ls_pipe_client_dtor(struct ls_pipe_client_win32 *pipe)
{
	if (pipe->hPipe)
		CloseHandle(pipe->hPipe);

	if (pipe->thread)
	{
		EnterCriticalSection(&pipe->thread->cs);
		pipe->thread->was_closed = 1;
		LeaveCriticalSection(&pipe->thread->cs);
	}
}

static int LS_CLASS_FN ls_pipe_client_wait(struct ls_pipe_client_win32 *pipe, unsigned long ms)
{
	DWORD dwResult;

	if (pipe->is_error) return -1;
	if (pipe->hPipe || !pipe->thread) return 0;

	dwResult = WaitForSingleObject(pipe->thread->hThread, ms);
	if (dwResult == WAIT_OBJECT_0)
		return 0;

	if (dwResult != WAIT_TIMEOUT)
		return -1;

	return 1;
}
#else

static void LS_CLASS_FN ls_pipe_client_dtor(void *ignored)
{
    // TODO: implement
}

static int LS_CLASS_FN ls_pipe_client_wait(void *ignored, unsigned long ms)
{
    // TODO: implement
    return 0;
}

#endif // LS_WINDOWS

static struct ls_class PipeServerClass = {
	.type = LS_PIPE,
#if LS_WINDOWS
	.cb = sizeof(struct ls_pipe_server_win32),
#endif
	.dtor = (ls_dtor_t)&ls_pipe_server_dtor,
	.wait = (ls_wait_t)&ls_pipe_server_wait
};

static struct ls_class PipeClientClass = {
	.type = LS_PIPE,
#if LS_WINDOWS
	.cb = sizeof(struct ls_pipe_client_win32),
#endif
	.dtor = (ls_dtor_t)&ls_pipe_client_dtor,
	.wait = (ls_wait_t)&ls_pipe_client_wait
};

#if LS_WINDOWS
static int ls_image_name(HANDLE hProcess, char *out, size_t size, char **name)
{
	WCHAR path[MAX_PATH];
	DWORD dwCount = MAX_PATH;
	BOOL bRet;
	int rc;
	char *p;

	bRet = QueryFullProcessImageNameW(hProcess, 0, path, &dwCount);
	if (!bRet) return 0;

	rc = ls_wchar_to_utf8_buf(path, out, (int)size);
	if (!rc)
	{
		*name = NULL;
		return 0;
	}

	p = strrchr(out, '\\');
	if (p) *name = p + 1;
	else *name = out;

	return rc;
}

static ls_handle ls_spawn_imp(LPWSTR cmd, LPWSTR env, LPCWSTR dir, int flags)
{
	struct ls_proc_win32 *ph;
	BOOL bRet;
	DWORD dwFlags = 0;

	ph = ls_handle_create(&ProcClass);
	if (!ph) return NULL;

	ph->si.cb = sizeof(STARTUPINFO);

	bRet = CreateProcessW(NULL, cmd, NULL, NULL, TRUE, dwFlags, env, dir, &ph->si, &ph->pi);
	if (!bRet)
	{
		ls_close(ph);
		return NULL;
	}

	ph->path_len = ls_image_name(ph->pi.hProcess, ph->path, sizeof(ph->path), &ph->name);

	return ph;
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

ls_handle ls_spawn(const char *path, const char *argv[], const char *envp[], const char *cwd, int flags)
{
#if LS_WINDOWS
	ls_handle ph = NULL;
	LPWSTR lpCmd = NULL;
	LPWSTR lpEnv = NULL;
	LPWSTR lpDir = NULL;

	lpCmd = ls_build_command_line(path, argv);
	lpEnv = ls_build_environment(envp);

	if (cwd)
		lpDir = ls_utf8_to_wchar(cwd);

	ph = ls_spawn_imp(lpCmd, lpEnv, lpDir, flags);

	ls_free(lpCmd);
	ls_free(lpEnv);
	ls_free(lpDir);
	return ph;
#else
    pid_t pid;
    struct ls_proc *proc;
    char **nargv, **nenvp;
    size_t len, i;
    int rc;
    pid_t child;
    int status;
    int exit_status;
    int pipefd[2]; // anon pipe
    char buf[1] = { 0 };
    ssize_t n;
    
    proc = ls_handle_create(&ProcClass);
    if (!proc)
        return NULL;
    
    rc = pipe(pipefd);
    if (rc == -1)
    {
        ls_close(proc);
        return NULL;
    }
    
    pid = fork();
    if (pid == -1)
    {
        close(pipefd[0]);
        close(pipefd[1]);
        
        ls_close(proc);
        return NULL;
    }
    
    if (pid == 0)
    {
        // child
        close(pipefd[0]);
        
        // close write pipe on execve
        rc = fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);
        if (rc == -1)
        {
            perror("fcntl");
            write(pipefd[1], buf, 1);
            exit(126);
        }
        
        if (cwd)
        {
            rc = chdir(cwd);
            if (rc == -1)
            {
                perror("chdir");
                write(pipefd[1], buf, 1);
                exit(126);
            }
        }
        
        // argv must not be const
        nargv = NULL;
        if (argv)
        {
            len = salen(argv);
            
            nargv = malloc((len + 1) * sizeof(char *));
            if (!nargv)
            {
                perror("malloc argv");
                write(pipefd[1], buf, 1);
                exit(126);
            }
            
            for (i = 0; i < len; i++)
            {
                nargv[i] = strdup(argv[i]);
                if (!nargv[i])
                {
                    perror("strdup argv");
                    write(pipefd[1], buf, 1);
                    exit(126);
                }
            }
            
            nargv[len] = NULL;
        }
        
        // envp must not be const
        nenvp = NULL;
        if (envp)
        {
            len = salen(envp);
            
            nenvp = malloc((len + 1) * sizeof(char *));
            if (!nenvp)
            {
                perror("malloc envp");
                write(pipefd[1], buf, 1);
                exit(126);
            }
            
            for (i = 0; i < len; i++)
            {
                nenvp[i] = strdup(envp[i]);
                if (!nenvp[i])
                {
                    perror("strdup envp");
                    write(pipefd[1], buf, 1);
                    exit(126);
                }
            }
            
            nenvp[len] = NULL;
        }
        
        execve(path, nargv, nenvp);
        
        perror(path);
        write(pipefd[1], buf, 1);
        exit(127);
    }
    
    // parent
    
    close(pipefd[1]);
    
    n = read(pipefd[0], buf, 1);
    close(pipefd[0]);
    
    if (n == -1)
    {
        // TODO: im not sure of the state of the child process here
        ls_close(proc);
        return NULL;
    }
    else if (n == 1)
    {
        // error
        (void)waitpid(pid, &status, 0);
        ls_close(proc);
        return NULL;
    }
    
    // execv succeeded
    
    proc->pid = pid;
    
    return proc;
#endif // LS_WINDOWS
}

ls_handle ls_spawn_shell(const char *cmd, const char *envp[], const char *cwd, int flags)
{
#if LS_WINDOWS
	ls_handle ph = NULL;
	LPWSTR lpCmd = NULL;
	LPWSTR lpEnv = NULL;
	LPWSTR lpDir = NULL;

	lpCmd = ls_utf8_to_wchar(cmd);
	lpEnv = ls_build_environment(envp);

	if (cwd)
		lpDir = ls_utf8_to_wchar(cwd);

	ph = ls_spawn_imp(lpCmd, lpEnv, lpDir, flags);

	ls_free(lpCmd);
	ls_free(lpEnv);
	ls_free(lpDir);
	return ph;
#else
    return NULL;
#endif // LS_WINDOWS
}

ls_handle ls_proc_open(unsigned long pid)
{
#if LS_WINDOWS
	struct ls_proc_win32 *ph;
	HANDLE hProcess;

	hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
	if (!hProcess) return NULL;

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
    return NULL;
#endif // LS_WINDOWS
}

void ls_kill(ls_handle ph, int signum)
{
#if LS_WINDOWS
	if (ph == LS_PROC_SELF) ExitProcess(signum);
	TerminateProcess(((struct ls_proc_win32 *)ph)->pi.hProcess, signum);
#else
    struct ls_proc *proc = ph;
    if (ph == LS_PROC_SELF) raise(SIGTERM);
    kill(proc->pid, 1);
#endif // LS_WINDOWS
}

int ls_proc_state(ls_handle ph)
{
#if LS_WINDOWS
	DWORD dwExitCode;

	if (ph == LS_PROC_SELF) return LS_PROC_RUNNING;

	if (!GetExitCodeProcess(((struct ls_proc_win32 *)ph)->pi.hProcess, &dwExitCode))
		return -1;

	if (dwExitCode == STILL_ACTIVE)
		return LS_PROC_RUNNING;

	return LS_PROC_TERMINATED;
#else
    return -1;
#endif // LS_WINDOWS
}

int ls_proc_exit_code(ls_handle ph, int *exit_code)
{
#if LS_WINDOWS
	DWORD dwExitCode;

	if (ph == LS_PROC_SELF) return 1;

	if (!GetExitCodeProcess(((struct ls_proc_win32 *)ph)->pi.hProcess, &dwExitCode))
		return -1;

	if (dwExitCode == STILL_ACTIVE)
		return 1;

	*exit_code = dwExitCode;
	return 0;
#else
    struct ls_proc *proc = ph;
    if (ph == LS_PROC_SELF) return 1;
    
    if (proc->status == 0)
        return 1;
    
    *exit_code = WEXITSTATUS(proc->status);
    return 0;
#endif // LS_WINDOWS
}

size_t ls_proc_path(ls_handle ph, char *path, size_t size)
{
#if LS_WINDOWS
	struct ls_proc_win32 *proc = (struct ls_proc_win32 *)ph;

	if (size == 0)
		return proc->path_len;

	if (size > proc->path_len)
		size = proc->path_len;

	memcpy(path, proc->path, size);
	return size;
#else
    return 0;
#endif // LS_WINDOWS
}

size_t ls_proc_name(ls_handle ph, char *name, size_t size)
{
#if LS_WINDOWS
	struct ls_proc_win32 *proc = (struct ls_proc_win32 *)ph;
	size_t len = proc->path_len - (proc->name - proc->path);

	if (size == 0)
		return len;

	if (size > len)
		size = len;

	memcpy(name, proc->name, size);
	return size;
#else
    return 0;
#endif // LS_WINDOWS
}

unsigned long ls_getpid(ls_handle ph)
{
#if LS_WINDOWS
	if (ph == LS_PROC_SELF) return GetCurrentProcessId();
	return ((struct ls_proc_win32 *)ph)->pi.dwProcessId;
#else
    struct ls_proc *proc = ph;
    if (ph == LS_PROC_SELF) return getpid();
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

ls_handle ls_proc_self(void) { return LS_PROC_SELF; }

unsigned long ls_proc_parent(unsigned long pid)
{
#if LS_WINDOWS
	HANDLE hSnapshot;
	PROCESSENTRY32W pe;
	DWORD dwParentPid = 0;

	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE) return -1;

	pe.dwSize = sizeof(PROCESSENTRY32W);
	if (!Process32FirstW(hSnapshot, &pe)) goto done;

	do
	{
		if (pe.th32ProcessID == pid)
		{
			dwParentPid = pe.th32ParentProcessID;
			break;
		}
	} while (Process32NextW(hSnapshot, &pe));

done:
	CloseHandle(hSnapshot);
	return dwParentPid;
#else
    return -1;
#endif // LS_WINDOWS
}

ls_handle ls_pipe_create(const char *name)
{
#if LS_WINDOWS
	struct ls_pipe_server_win32 *ph;
	BOOL bRet;
	DWORD dwError;

	ph = ls_handle_create(&PipeServerClass);
	if (!ph) return NULL;

	ls_utf8_to_wchar_buf(name, ph->szPipeName, LS_PIPE_NAME_SIZE);	
	
	ph->hPipe = CreateNamedPipeW(ph->szPipeName,
		PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
		PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
		PIPE_UNLIMITED_INSTANCES, 4096, 4096, 0, NULL);

	if (!ph->hPipe)
	{
		ls_close(ph);
		return NULL;
	}

	ph->ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
	if (!ph->ov.hEvent)
	{
		ls_close(ph);
		return NULL;
	}

	bRet = ConnectNamedPipe(ph->hPipe, &ph->ov);
	dwError = GetLastError();
	if (dwError != ERROR_IO_PENDING && dwError != ERROR_PIPE_CONNECTED)
	{
		ls_close(ph);
		return NULL;
	}

	return ph;
#else
    return NULL;
#endif
}

ls_handle ls_pipe_open(const char *name, unsigned long ms)
{
#if LS_WINDOWS
	struct ls_pipe_client_win32 *ph;
	struct ls_pipe_client_thread *thread;
	int rc;

	ph = ls_handle_create(&PipeClientClass);
	if (!ph) return NULL;

	ls_utf8_to_wchar_buf(name, ph->szPipeName, LS_PIPE_NAME_SIZE);

	rc = ls_pipe_open_now(ph->szPipeName, &ph->hPipe, ms);
	if (rc == -1)
	{
		ls_close(ph);
		return NULL;
	}

	if (rc == 0)
		return ph;

	thread = ls_malloc(sizeof(struct ls_pipe_client_thread));
	if (!thread)
	{
		ls_close(ph);
		return NULL;
	}

	thread->was_closed = 0;

	wcscpy_s(ph->szPipeName, LS_PIPE_NAME_SIZE, thread->szPipeName);

	InitializeCriticalSection(&thread->cs);

	thread->is_error = &ph->is_error;
	thread->phPipe = &ph->hPipe;

	thread->hThread = CreateThread(NULL, 0, &ls_pipe_client_wait_thread, thread, 0, NULL);
	if (!thread->hThread)
	{
		ls_free(thread);
		ls_close(ph);
		return NULL;
	}

	ph->thread = thread;

	return ph;
#else
    return NULL;
#endif
}
