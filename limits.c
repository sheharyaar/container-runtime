#include <errno.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "container.h"
#define CGROUP_DIR "lagnos"

int setup_limits(container_ctx* ctx)
{
	// setup cgroup limit in directory and then set ctx->cgroup_fd to the cgroup fd
	// for clone flag
	char cgroup_path[128];
	char memcg_path[156];
	char memcg_swap[156];
	char memcg_zswap[156];
	int fd, mem_fd;

	snprintf(cgroup_path, 128, "/sys/fs/cgroup/%s", CGROUP_DIR);
	memcpy(ctx->cgrp_path, cgroup_path, 128);
	snprintf(memcg_path, 156, "%s/memory.max", cgroup_path);
	snprintf(memcg_swap, 156, "%s/memory.swap.max", cgroup_path);
	snprintf(memcg_zswap, 156, "%s/memory.zswap.max", cgroup_path);
	pr_info("cgroup path: %s\n", cgroup_path);
	pr_info("memcg path: %s\n", memcg_path);
	pr_info("memcg swap path: %s\n", memcg_swap);
	pr_info("memcg zswap path: %s\n", memcg_zswap);

	if (mkdir(cgroup_path, O_RDWR | O_CLOEXEC) < 0) {
		pr_err("error in making cgroup directory: %s\n", strerror(errno));
		return -1;	
	}

	fd = open(cgroup_path, O_DIRECTORY | O_CLOEXEC);
	if (fd == -1) {
		pr_err("error in open cgroup dir: %s\n", strerror(errno));
		return -1;
	}

	// write memory limit to file
	mem_fd = open(memcg_path, O_WRONLY | O_CLOEXEC);
	if (mem_fd == -1) {
		pr_err("error in opening memcg file: %s\n", strerror(errno));
		goto clean_cgroup;
	}
	
	if(write(mem_fd, ctx->mem_max, MAX_MEMCG_LEN) < 0) {
		pr_err("error in writing mem limit: %s\n", strerror(errno));
		goto clean_memcg;
	}
	pr_info("memory limit %s written to memory cgroup file!\n", ctx->mem_max);
	close(mem_fd);

	// write swap memory limit to file
	mem_fd = open(memcg_swap, O_WRONLY | O_CLOEXEC);
	if (mem_fd == -1) {
		pr_err("error in opening memcg swap: %s\n", strerror(errno));
		goto clean_cgroup;
	}
	
	if(write(mem_fd, "0", 1) < 0) {
		pr_err("error in memory swap: %s\n", strerror(errno));
		goto clean_memcg;
	}
	pr_info("memory swap disabled\n");
	close(mem_fd);

	// write zswap memory limit to file
	mem_fd = open(memcg_zswap, O_WRONLY | O_CLOEXEC);
	if (mem_fd == -1) {
		pr_err("error in opening memcg zswap: %s\n", strerror(errno));
		goto clean_cgroup;
	}
	
	if(write(mem_fd, "0", 1) < 0) {
		pr_err("error in memory zswap: %s\n", strerror(errno));
		goto clean_memcg;
	}
	pr_info("memory zswap disabled\n");
	close(mem_fd);

	ctx->cgroup_fd = fd;
	return 0;

clean_memcg:
	close(mem_fd);
clean_cgroup:
	close(fd);
	return -1;
}

