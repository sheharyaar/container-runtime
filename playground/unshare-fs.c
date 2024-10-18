#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define pr_err(...) fprintf(stderr, "[!] " __VA_ARGS__)
#define pr_info(...) fprintf(stdout, "[*] " __VA_ARGS__)

#define STACK_SIZE (1024 * 1024)

int child_chroot(void* args)
{
    int unshared = *(int*)args;
    char dir[1024];

    if (unshared > 0) {
        pr_info("executing unshare!\n");
        if (unshare(CLONE_FS) == -1) {
            pr_err("error in unshare: %s\n", strerror(errno));
            return -1;
        }
    }

    pr_info("performing chroot!\n");
    if (chroot("/home/wazir") == -1) {
        pr_err("error in chroot: %s\n", strerror(errno));
        return -1;
    }

    pr_info("child cwd: %s\n", getcwd(dir, 1024));
    return 0;
}

int clone_and_chroot(int unshared)
{
    char* stack = mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE,
        MAP_ANONYMOUS | MAP_STACK | MAP_PRIVATE, -1, 0);
    if (stack == MAP_FAILED) {
        pr_err("error in mmap: %s\n", strerror(errno));
        return -1;
    }

    int child = clone(
        child_chroot, stack + STACK_SIZE, CLONE_FS | SIGCHLD, &unshared);
    if (child == -1) {
        pr_err("error in clone: %s\n", strerror(errno));
        return -1;
    }

    sleep(2);
    if (waitpid(child, NULL, 0) == -1) {
        pr_err("error in waitpid: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

int main(int argc, char* argv[])
{
    if (argc < 3) {
        pr_err("Incomplete command\nUsage: ./unshare-fs --unshare [0,1]\n");
        return 1;
    }

    char dir[1024];
    pr_info("parent cwd: %s\n", getcwd(dir, 1024));
    memset(dir, 0, 1024);

    int unshared = atoi(argv[2]);
    pr_info(
        "cloning a child %s unshare\n", unshared > 0 ? "_with_" : "_without_");

    clone_and_chroot(unshared);

    pr_info("parent cwd: %s\n", getcwd(dir, 1024));
    memset(dir, 0, 1024);
    return 0;
}