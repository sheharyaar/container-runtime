#define _GNU_SOURCE
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/capability.h>
#include <sys/wait.h>
#include <syscall.h>
#include <linux/sched.h>
#include <stdint.h>

#include "container.h"

void print_usage()
{
        fprintf(stdout, "Usage:\n");
        fprintf(stdout,
            "# ./container --rootfs <rootfs_path> --memory <memory_limit> -- <command> "
            "[command_args]\n");
}

container_ctx* ctx_new(void)
{
        container_ctx* ctx = malloc(sizeof(container_ctx));
        if (ctx == NULL) {
                pr_err("cannot allocate context, not enough mmeory");
                return NULL;
        }

        ctx->child_pid = -1;
        ctx->parent_pid = -1;
	ctx->cgroup_fd = -1;
        ctx->flags = 0;
        ctx->args = NULL;
        return ctx;
}

// parse options and set parameters
int parse_cmd(int argc, char* argv[], container_ctx* ctx)
{
        if (argc < 7) {
                pr_err("insufficient arguments\n");
        }

        if (strncmp(argv[1], "--rootfs", strlen("--rootfs")) != 0) {
                return -1;
        }

        if (strncmp(argv[3], "--memory", strlen("--memory")) != 0) {
                return -1;
        }

        if (strncmp(argv[5], "--", 2) != 0) {
                return -1;
        }

        strncpy(ctx->rootfs, argv[2], MAX_PATH_LEN);
        if (ctx->rootfs[0] == '\0' || ctx->rootfs[0] != '/') {
                pr_err("invalid rootfs path, path should be absolute");
                return -1;
        }
        ctx->rootfs[MAX_PATH_LEN] = '\0';

        strncpy(ctx->mem_max, argv[4], MAX_MEMCG_LEN);
        if (ctx->rootfs[0] == '\0' || ctx->rootfs[0] != '/') {
                pr_err("invalid rootfs path, path should be absolute");
                return -1;
        }
        ctx->mem_max[MAX_MEMCG_LEN] = '\0';

	ctx->args = &argv[6];

        ctx->working_dir[0] = '/';
        ctx->working_dir[1] = '\0';
        return 0;
}

int handle_child(container_ctx *ctx) {
	if (setup_rootfs_child(ctx) != 0) {
		pr_err("error in setting up rootfs\n");
		return -1;
	}
	pr_info("rootfs setup succesful\n");

	pr_info("execing the program\n");
	if (execve(ctx->args[0], ctx->args, NULL) == -1) {
		pr_err("error in execve: %s\n", strerror(errno));
		return -1;
	}
	
	return 0;
}

int handle_parent(container_ctx *ctx) {
	/* LOOK into CLONE_PIDFD for sync and namespace purposes
	if (setup_network(ctx) < 0) {
		pr_err("error in setup network\n");
		return -1;
	}
	pr_info("veth created succesfully\n");
	*/
	if (waitpid(ctx->child_pid, NULL, 0) == -1) {
		pr_err("error in waitpid: %s\n", strerror(errno));
		/* TODO: destroy veth ?? */
		return -1;
	}
 	return 0;
}

/**
 * Sets up the container :
 * - Sets up the clone flags
 * - Sets up the network
 * - Sets up the file system
 * - Sets up cgroup limits
 * - Sets up tty and environment variables
 * - Remove CAP_SYS_ADMIN from the child process.
 */
int container_setup(container_ctx* ctx)
{
	/* TODO: CLONE_NEWUSER ?? */
        uint64_t flags = CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWIPC
            | CLONE_NEWNET | CLONE_INTO_CGROUP;
        ctx->flags = flags;

	// we will use clone3 to use CLONE_INTO_CGROUP flag, this will allow us
	// to create the child directly into the cgroup
	if (setup_limits(ctx) < 0) {
		pr_err("error in setting cgroup limits\n");
		return -1;
	}
	pr_info("cgroup fd: %zu\n", ctx->cgroup_fd);

	struct clone_args args = {0};
	args.flags = ctx->flags;
	args.cgroup = ctx->cgroup_fd;
	args.exit_signal = SIGCHLD;

        int pid = syscall(SYS_clone3, &args, sizeof(struct clone_args));
        if (pid == -1) {
                pr_info("error in clone: %s\n", strerror(errno));
                return -1;
        } else if (pid == 0) {
                /* child */
		return handle_child(ctx);
	} else {
                /* parent */
		pr_info("parent_pid: %d\n", getpid());
		pr_info("child_pid: %d\n", pid);
		ctx->child_pid = pid;
		return handle_parent(ctx);
	}
}

int main(int argc, char* argv[])
{
        container_ctx* ctx = ctx_new();
        if (ctx == NULL)
                exit(EXIT_FAILURE);

        if (parse_cmd(argc, argv, ctx) != 0) {
                pr_err("error iin parsing command line options\n");
                print_usage();
                exit(EXIT_FAILURE);
        }

        ctx->parent_pid = getpid();

        // check for CAP_SYS_ADMIN needed for CLONE_NEWPID
        cap_t caps = cap_get_proc();
        if (caps == NULL) {
                pr_err("error in getting capabilities: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
        }

        cap_flag_value_t val;
        if (cap_get_flag(caps, CAP_SYS_ADMIN, CAP_EFFECTIVE, &val) == -1) {
                pr_err("error in getting CAP_SYS_ADMIN value: %s\n",
                    strerror(errno));
                exit(EXIT_FAILURE);
        }

        if (val != CAP_SET) {
                pr_err("CAP_SYS_ADMIN ability not available, needed for "
                       "creating PID "
                       "namespace\n");
                exit(EXIT_FAILURE);
        }
        pr_info("capability CAP_SYS_ADMIN present\n");

        if (container_setup(ctx) == -1)
                exit(EXIT_FAILURE);

        exit(EXIT_SUCCESS);
}
