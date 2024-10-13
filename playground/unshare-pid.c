#define _GNU_SOURCE
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define STACK_SIZE (1024 * 1024)

static int print_pid(void* args)
{
    fprintf(
        stderr, "[child] PID: %d\n[child] Thread id: %d\n", getpid(), gettid());
    return 0;
}

int clone_and_pid(void)
{
    char* stack = mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (stack == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        return -1;
    }

    int pid = clone(print_pid, stack + STACK_SIZE, 0, NULL);
    if (pid == -1) {
        fprintf(stderr, "error in clone: %s\n", strerror(errno));
        munmap(stack, STACK_SIZE); // Clean up stack on failure
        return -1;
    }

    waitpid(pid, NULL, 0);
    munmap(stack, STACK_SIZE);
    return 0;
}

int main()
{
    fprintf(stdout, "[parent] PID: %d\n", getpid());

    if (clone_and_pid() != 0)
        return 1;

    // sleep to let the child be scheduled
    sleep(2);

    int ret = unshare(CLONE_NEWPID);
    if (ret == -1) {
        fprintf(stderr, "error in unshare: %s\n", strerror(errno));
        return 1;
    }

    fprintf(stdout,
        "[!] unshare executed successfully, children will start from PID 1\n");

    fprintf(stdout, "[!] Cloning a child\n");
    if (clone_and_pid() != 0)
        return 1;

    // sleep to let the child be scheduled
    sleep(2);

    /*
        This call should fail, since the first cloned process after
       unshare acts as init(1) of the new namespace. Now since the init (or the
       previous process) has already exited, we cannot create a new process in
       the new namespace. For more information, see: unshare(2) manpage -- The
       namespace init process section.

        Quoted: " if the first child subsequently
       created by a fork(2) terminates, then subsequent calls to fork(2) fail
       with ENOMEM "
    */
    fprintf(stdout,
        "[!] Cloning a child again, this SHOULD FAIL. See comments in the "
        "source code !\n");

    if (clone_and_pid() != 0)
        return 1;

    return 0;
}
