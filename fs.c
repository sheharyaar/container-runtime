#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <syscall.h>
#include <unistd.h>
#include <sched.h>

#include "container.h"

static int update_map(char *path, char *buf, size_t size) {
	int fd = open(path, O_WRONLY | O_CLOEXEC);
	if (fd == -1) {
		pr_err("error in opening file (%s): %s\n", path, strerror(errno));
		return -1;
	}

	if (write(fd, buf, size) < 0) {
		pr_err("error in writing to file (%s): %s\n", path, strerror(errno));
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}

int setup_uid_gid_map(container_ctx *ctx)
{
	// open the uid_map file and write the string
	// we will use hardcoded value from /proc/$$/uid_map
	char uid_map_path[256];
	char gid_map_path[256];
	char *map_buf = "0 0 4294967295";

	snprintf(uid_map_path, 255, "/proc/%d/uid_map", ctx->child_pid);
	snprintf(gid_map_path, 255, "/proc/%d/gid_map", ctx->child_pid);

	if (update_map(uid_map_path, map_buf, strlen(map_buf)) < 0) return -1;
	if (update_map(gid_map_path, map_buf, strlen(map_buf)) < 0) return -1;
	return 0;
}

/* sets up the filesystem, called from the child process */
int setup_rootfs_childns(container_ctx* ctx)
{
        int ret = 1;

        // mount the current fs as MS_PRIVATE to prevent child mounts from propagating
        if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) == -1) {
                pr_err("mount: %s\n", strerror(errno));
                return -1;
        }

	// ensure the new root mount point is a mount point, by doing a MS_BIND mount
        if (mount(ctx->rootfs, ctx->rootfs, NULL, MS_BIND, NULL) == -1) {
                pr_err("mount: %s\n", strerror(errno));
                return -1;
        }

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
                goto close_new;
        }

	// perform pivot root
        if (syscall(SYS_pivot_root, ".", ".") == -1) {
                pr_err("pivot_root: %s\n", strerror(errno));
                goto close_new;
        }

	// mount the procfs before we unmount the oldroot
	if (mount("proc", "/proc", "proc", MS_NOSUID|MS_NOEXEC|MS_NODEV, NULL) < 0) {
		pr_err("error in mounting procfs: %s\n", strerror(errno));
		return -1;
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
		
        ret = 0;
close_new:
        close(newroot);
close_old:
        close(oldroot);

        return ret;
}
