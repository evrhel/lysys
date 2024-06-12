/**
 * Fibers are a lightweight alternative to threads. They are similar to threads
 * in that they can run concurrently. However, they are not scheduled by the
 * operating system, instead being explicitly scheduled by by the application.
 * Fibers are useful for implementing coroutines, cooperative multitasking, and
 * other tasks that require a high degree of control over the execution of the
 * program.
 * 
 * See ls_thread.h for more information on fibers.
*/

#include <stdio.h>
#include <stdlib.h>

#include <lysys/lysys.h>

static int fiber1_func(void *up)
{
    printf("fiber1\n");
    ls_fiber_sched(); // Switch to the main fiber.
    return 0;
}

static int fiber2_func(void *up)
{
    printf("fiber2\n");
    ls_fiber_sched(); // Switch to the main fiber.
    return 0;
}

int main(int argc, char *argv[])
{
    ls_handle fiber1, fiber2;
    int rc;

    // Convert the main thread to a fiber
    rc = ls_convert_to_fiber(NULL);
    if (rc == -1)
    {
        ls_perror("ls_convert_to_fiber");
        exit(1);
    }

    // Create the fibers
    fiber1 = ls_fiber_create(&fiber1_func, NULL);
    if (!fiber1)
    {
        ls_perror("ls_fiber_create");
        exit(1);
    }

    fiber2 = ls_fiber_create(&fiber2_func, NULL);
    if (!fiber2)
    {
        ls_perror("ls_fiber_create");
        exit(1);
    }

    // Switch to the first fiber
    // Returns when fiber1_func calls ls_fiber_sched
    ls_fiber_switch(fiber1);

    // Switch to the second fiber
    // Returns when fiber2_func calls ls_fiber_sched
    ls_fiber_switch(fiber2);

    // Clean up
    ls_close(fiber2);
    ls_close(fiber1);

    rc = ls_convert_to_thread();
    if (rc == -1)
    {
        ls_perror("ls_convert_to_thread");
        exit(1);
    }

    return 0;
}
