#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <syscall.h>
#include <unistd.h>

#include "container.h"

/* sets up the filesystem, called from the child process */
int setup_rootfs_child(container_ctx* ctx)
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
                goto close_new;
        }

        // ensure that the new root mount point is PRIVATE
        if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) == -1) {
                pr_err("mount: %s\n", strerror(errno));
                goto close_new;
        }

        // ensure the new root mount point is a mount point
        if (mount(ctx->rootfs, ctx->rootfs, NULL, MS_BIND, NULL) == -1) {
                pr_err("mount: %s\n", strerror(errno));
                goto close_new;
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

        // make the oldroot private to prevent the container unmount the oldroot
        // in host
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

        if (mount(
                "proc", "/proc", "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL)
            == -1) {
                pr_err("proc mount: %s\n", strerror(errno));
                goto close_new;
        }

        ret = 0;
close_new:
        close(newroot);
close_old:
        close(oldroot);

        return ret;
}
