#ifndef __CONTAINER_H
#define __CONTAINER_H

#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sched.h>
#include <stdint.h>
#include <unistd.h>

#define MAX_PATH_LEN 256
#define MAX_MEMCG_LEN 32

#define pr_err(...) fprintf(stderr, "[!] " __VA_ARGS__);
#define pr_info(...) fprintf(stdout, "[*] " __VA_ARGS__);

typedef struct container_s {
        char** args;
        char rootfs[MAX_PATH_LEN + 1];
        char working_dir[MAX_PATH_LEN + 1];
	char cgrp_path[MAX_PATH_LEN + 1];
	char mem_max[MAX_MEMCG_LEN + 1];
        uint64_t flags;
        uint64_t cgroup_fd;
        pid_t parent_pid;
        pid_t child_pid;
} container_ctx;

int setup_network(container_ctx *ctx);

int setup_uid_gid_map(container_ctx *ctx);
int setup_rootfs_childns(container_ctx* ctx);

int setup_limits(container_ctx* ctx);

#endif
