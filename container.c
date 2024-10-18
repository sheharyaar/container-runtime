#define _GNU_SOURCE
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sys/capability.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <syscall.h>
#include <unistd.h>

#define pr_err(...) fprintf(stderr, "[!] " __VA_ARGS__);
#define pr_info(...) fprintf(stdout, "[*] " __VA_ARGS__);

int clone_and_exec_child(int argc, char* args[])
{
    pr_info("executing clone\n");
    // TODO: TIME, CGROUP and USER namespaces
    int flags = CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWIPC
        | CLONE_NEWNET;
    int pid = syscall(SYS_clone, SIGCHLD | flags, 0, NULL, NULL, 0);

    if (pid == -1) {
        pr_info("error in clone: %s\n", strerror(errno));
        return -1;
    } else if (pid == 0) {
        // child
        // TODO: use pivot_root
        if (chroot("/") == -1) {
            pr_err("error in chroot: %s\n", strerror(errno));
            return -1;
        }

        // mounting proc without CLONE_NEWNS can cause issues, for me it caused
        // segfault in libsystemd-shared
        // TODO: look into mount flags for mor info
        mount("", "/", NULL, MS_PRIVATE | MS_REC, NULL);
        mount("proc", "/proc", "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL);

        pr_info("execing the program\n");
        if (execve(args[0], args, environ) == -1) {
            pr_err("error in execle: %s\n", strerror(errno));
            return -1;
        }
    } else {
        // parent
        if (waitpid(pid, NULL, 0) == -1) {
            pr_err("error in waitpid: %s\n", strerror(errno));
            return -1;
        }
    }
    return 0;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        pr_err("Invalid argument\nUsage: ./container <program>\n");
        return 1;
    }

    // check for CAP_SYS_ADMIN needed for CLONE_NEWPID
    cap_t caps = cap_get_proc();
    if (caps == NULL) {
        pr_err("error in getting capabilities: %s\n", strerror(errno));
        return 1;
    }

    cap_flag_value_t val;
    if (cap_get_flag(caps, CAP_SYS_ADMIN, CAP_EFFECTIVE, &val) == -1) {
        pr_err("error in getting CAP_SYS_ADMIN value: %s\n", strerror(errno));
        return 1;
    }

    if (val != CAP_SET) {
        pr_err("CAP_SYS_ADMIN ability not available, needed for creating PID "
               "namespace\n");
        return 1;
    }

    pr_info("capability CAP_SYS_ADMIN present\n");
    if (clone_and_exec_child(argc - 1, ++argv) == -1)
        return 1;

    return 0;
}