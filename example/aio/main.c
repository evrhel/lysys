#include <lysys/lysys.h>

int main(int argc, char *argv[])
{
    ls_handle file;
    ls_handle aio;
    int rc;
    void *buf;
    struct ls_stat st;

    // Open a file for reading asynchronously.
    file = ls_open("large.bin", LS_FILE_READ | LS_FLAG_ASYNC,
        LS_SHARE_READ, LS_OPEN_EXISTING);
    if (!file)
    {
        ls_perror("ls_open");
        exit(1);
    }

    // Get the size of the file.
    rc = ls_fstat(file, &st);
    if (rc == -1)
    {
        ls_perror("ls_fstat");
        exit(1);
    }

    // Allocate a buffer to read the file into.
    buf = ls_malloc(st.size);
    if (!buf)
    {
        ls_perror("ls_malloc");
        exit(1);
    }

    // Create an asynchronous I/O handle.
    aio = ls_aio_open(file);
    if (!aio)
    {
        ls_perror("ls_aio_open");
        exit(1);
    }

    // Dispatch an asynchronous read operation.
    rc = ls_aio_read(aio, 0, buf, st.size);
    if (rc == -1)
    {
        ls_perror("ls_aio_read");
        exit(1);
    }

    // Pretend to do something else.
    ls_sleep(1000);

    // Wait for the read to complete.
    rc = ls_wait(aio);
    if (rc == -1)
    {
        ls_perror("ls_wait");
        exit(1);
    }

    printf("File read!\n");

    ls_close(aio);
    ls_close(file);

    return 0;
}
