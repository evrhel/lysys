#include <lysys/lysys.h>

static const char string[] = "Hello, world!";

int main(int argc, char *argv[])
{
    char buf[sizeof(string)];
    intptr_t fmt;
    int rc;
    size_t len;

    // Register a new clipboard format.
    fmt = ls_register_clipboard_format("my_format");
    if (fmt == -1)
    {
        ls_perror("ls_register_clipboard_format");
        exit(1);
    }

    // Set the data in our format.
    rc = ls_set_clipboard_data(fmt, string, sizeof(string));
    if (rc == -1)
    {
        ls_perror("ls_set_clipboard_data");
        exit(1);
    }

    // Get the data back.
    len = ls_get_clipboard_data(fmt, buf, sizeof(buf));
    if (len == -1)
    {
        ls_perror("ls_get_clipboard_data");
        exit(1);
    }

    // Our data was likely overwritten by another application.
    if (len == 0)
    {
        printf("Data not available in requested format\n");
        return 0;
    }

    // Print the data.
    printf("Data: %s\n", buf);

    return 0;
}
