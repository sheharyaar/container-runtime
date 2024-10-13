#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define STACK_SIZE (1024 * 1024)

int child_handler(void* args)
{
    int* _args = (int*)args;
    int ret;
    fprintf(stdout, "\t[!] child PID: %d\n", getpid());

    if (_args[0]) {
        fprintf(stdout, "[!] unsharing CLONE_FILES\n");
        if (unshare(CLONE_FILES) == -1) {
            fprintf(stderr, "error in unshare: %s\n", strerror(errno));
            return -1;
        }
    }

    fprintf(stdout, "\t[!] closing the fd (%d) in the child\n", _args[1]);
    if (close(_args[1]) == -1) {
        fprintf(stderr, "error in close: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int clone_child_and_move(int unshared, int pid)
{
    char* stack = mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE,
        MAP_STACK | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stack == MAP_FAILED) {
        fprintf(stderr, "error in mmap: %s\n", strerror(errno));
        return -1;
    }

    int args[2] = { unshared, pid };

    int child
        = clone(child_handler, stack + STACK_SIZE, CLONE_FILES | SIGCHLD, args);
    if (child == -1) {
        fprintf(stderr, "error in clone: %s\n", strerror(errno));
        return -1;
    }

    sleep(4);
    waitpid(child, NULL, 0);
    munmap(stack, STACK_SIZE);
    return 0;
}

int main(int argc, char* argv[])
{
    /* This program craetes moves the file offset by 2 characters, then
    clones a child and moves the file offset by 2 characters in the child and
    checks if it affects the position in the parent. This is tested before
    unshare and after unshare.*/
    if (argc < 3) {
        fprintf(stderr,
            "error incomplete arguments\nusage: ./unshare-files --unshare "
            "[1,0]\n");
        return 1;
    }

    fprintf(stdout,
        "Expected behavour --\n"
        "With unshare: the write should be successful by the parent\n"
        "Without unshare: the write should error out bad descriptor\n");

    int unshared = atoi(argv[2]);
    int ret;
    fprintf(stdout, "[!] parent PID: %d\n", getpid());
    fprintf(stdout, "[!] opening test.txt\n");

    int fd = open("test.txt", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
    if (fd == -1) {
        fprintf(stderr, "error in opening file \"test.txt\": %s\n",
            strerror(errno));
        return 1;
    }

    fprintf(stdout, "[!] cloning a child %s unsharing\n",
        unshared ? "_with_" : "_without_");
    ret = clone_child_and_move(unshared, fd);
    if (ret == -1)
        return 1;

    fprintf(stdout, "[!] attempting to write to fd in the parent\n");

    char buf[12] = "hello world\n";
    if (write(fd, buf, 12) == -1) {
        fprintf(stderr, "error in writing to file: %s\n", strerror(errno));
        return 1;
    }

    fprintf(stdout, "[!] wrote to test.txt successfully\n");

    return 0;
}