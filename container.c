#define _GNU_SOURCE
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/capability.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <syscall.h>
#include <unistd.h>

#define MAX_PATH_LEN 256

#define pr_err(...) fprintf(stderr, "[!] " __VA_ARGS__);
#define pr_info(...) fprintf(stdout, "[*] " __VA_ARGS__);

typedef struct container_s {
    char** args;
    char rootfs[MAX_PATH_LEN + 1];
    char working_dir[MAX_PATH_LEN + 1];
    int flags;
} container_ctx;

void print_usage()
{
    fprintf(stdout, "Usage:\n");
    fprintf(stdout,
        "# ./container --rootfs <rootfs_path> -- <command> [command_args]\n");
}

// parse options and set parameters
int parse_cmd(int argc, char* argv[], container_ctx* ctx)
{
    if (argc < 5) {
        pr_err("insufficient arguments\n");
    }

    if (strncmp(argv[1], "--rootfs", strlen("--rootfs")) != 0) {
        return -1;
    }

    strncpy(ctx->rootfs, argv[2], MAX_PATH_LEN);
    if (ctx->rootfs[0] == '\0' || ctx->rootfs[0] != '/') {
        pr_err("invalid rootfs path, path should be absolute");
        return -1;
    }
    ctx->rootfs[MAX_PATH_LEN] = '\0';

    if (strncmp(argv[3], "--", 2) != 0) {
        return -1;
    }

    ctx->args = &argv[4];

    ctx->working_dir[0] = '/';
    ctx->working_dir[1] = '\0';
    return 0;
}

// setup networking
int setup_network(container_ctx* ctx) { return 0; }

// setup file system
int setup_rootfs(container_ctx* ctx)
{
    int ret = 1;

    // get handles for oldroot and newroot
    int oldroot = open("/", O_DIRECTORY | O_RDONLY, 0);
    if (oldroot == -1) {
        pr_err("oldroot open: %s\n", strerror(errno));
        return -1;
    }

    pr_info("opening rootfs: %s\n", ctx->rootfs);
    int newroot = open(ctx->rootfs, O_DIRECTORY | O_RDONLY, 0);
    if (newroot == -1) {
        pr_err("newroot open: %s\n", strerror(errno));
        goto close_old;
    }

    // change directory to the new root to perform pivot
    if (fchdir(newroot) == -1) {
        pr_err("fchdir: %s\n", strerror(errno));
        return -1;
    }

    // ensure that the new root mount point is PRIVATE
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) == -1) {
        pr_err("mount: %s\n", strerror(errno));
        return -1;
    }

    // ensure the new root mount point is a mount point
    if (mount(ctx->rootfs, ctx->rootfs, NULL, MS_BIND, NULL) == -1) {
        pr_err("mount: %s\n", strerror(errno));
        return -1;
    }

    // perform pivot root
    if (syscall(SYS_pivot_root, ".", ".") == -1) {
        pr_err("pivot_root: %s\n", strerror(errno));
        goto close_new;
    }

    /**
     * go back to oldroot. Why ?? : we need to unmount the oldroot,
     * acccording to pivot_root, oldroot is underneath the newroot,
     * so we need to be out of oldroot to unmount it.
     **/
    if (fchdir(oldroot) == -1) {
        pr_err("oldroot fchdir: %s\n", strerror(errno));
        goto close_new;
    }

    // make the oldroot private to prevent the container unmount the oldroot in
    // host
    if (mount("", ".", "", MS_PRIVATE | MS_REC, NULL) == -1) {
        pr_err("mount: %s\n", strerror(errno));
        goto close_new;
    }

    // unmount the old one
    if (umount2(".", MNT_DETACH) == -1) {
        pr_err("umount: %s\n", strerror(errno));
        goto close_new;
    }

    // go to the new root
    if (chdir("/") == -1) {
        pr_err("chrdir: %s\n", strerror(errno));
        goto close_new;
    }

    if (mount("proc", "/proc", "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL)
        == -1) {
        pr_err("proc mount: %s\n", strerror(errno));
        goto close_new;
    }

    ret = 0;
close_old:
    close(oldroot);
close_new:
    close(newroot);

    return ret;
}

// setup clone flags and cgroup limits
void setup_clone_flags(container_ctx* ctx)
{
    int flags = CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWIPC
        | CLONE_NEWNET;
    ctx->flags = flags;
}

int setup_limits(container_ctx* ctx) { return 0; }

int container_run(container_ctx* ctx)
{
    setup_clone_flags(ctx);
    pr_info("container clone flags set\n");

    if (setup_network(ctx) != 0) {
        pr_err("error  in setting up network\n");
        /* TODO: goto unmount ?? */
        return -1;
    }
    pr_info("network setup succesful\n");

    if (setup_limits(ctx) != 0) {
        pr_err("error in setting limits\n");
        /* TODO: goto network and mount cleanup ?? */
        return -1;
    }
    pr_info("limits setup succesful\n");

    int pid = syscall(SYS_clone, SIGCHLD | ctx->flags, 0, NULL, NULL, 0);

    if (pid == -1) {
        pr_info("error in clone: %s\n", strerror(errno));
        return -1;
    } else if (pid == 0) {
        /* child */
        /* TODO: See environment passing */
        if (setup_rootfs(ctx) != 0) {
            pr_err("error in setting up rootfs\n");
            return -1;
        }
        pr_info("rootfs setup succesful\n");

        pr_info("execing the program\n");
        if (execve(ctx->args[0], ctx->args, environ) == -1) {
            pr_err("error in execve: %s\n", strerror(errno));
            return -1;
        }
    } else {
        /* parent */
        if (waitpid(pid, NULL, 0) == -1) {
            pr_err("error in waitpid: %s\n", strerror(errno));
            /* TODO: goto destroy network and unmount ?? */
            return -1;
        }
    }

    return 0;
}

int main(int argc, char* argv[])
{
    container_ctx ctx;
    if (parse_cmd(argc, argv, &ctx) != 0) {
        pr_err("error iin parsing command line options\n");
        print_usage();
        exit(EXIT_FAILURE);
    }

    // check for CAP_SYS_ADMIN needed for CLONE_NEWPID
    cap_t caps = cap_get_proc();
    if (caps == NULL) {
        pr_err("error in getting capabilities: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    cap_flag_value_t val;
    if (cap_get_flag(caps, CAP_SYS_ADMIN, CAP_EFFECTIVE, &val) == -1) {
        pr_err("error in getting CAP_SYS_ADMIN value: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (val != CAP_SET) {
        pr_err("CAP_SYS_ADMIN ability not available, needed for creating PID "
               "namespace\n");
        exit(EXIT_FAILURE);
    }
    pr_info("capability CAP_SYS_ADMIN present\n");

    if (container_run(&ctx) == -1)
        exit(EXIT_FAILURE);

    exit(EXIT_SUCCESS);
}
