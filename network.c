/* This is incomplete, I have little experience with netlink, will finish this once
 * I am more experienced with netlink and virtual networking. This can also be done
 * as part of building a CNI. */

#define _GNU_SOURCE

#include <netlink/cache.h>
#include <netlink/socket.h>
#include "container.h"
#include <asm/types.h>
#include <linux/if.h>
#include <linux/netlink.h>
#include <fcntl.h>
#include <linux/rtnetlink.h>
#include <netlink/netlink.h>
#include <netlink/route/addr.h>
#include <netlink/route/link.h>
#include <netlink/route/link/bridge.h>
#include <netlink/route/link/veth.h>
#include <netlink/route/rtnl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <sched.h>

int configure_interface(pid_t pid, const char* if_name, const char* ip_address)
{
        int ret = -1;
	int ns_fd = -1;
	char ns_path[256];
        struct nl_cache* cache = NULL;
	struct nl_sock *sock = NULL;
        struct rtnl_link *link = NULL;
        struct rtnl_addr* addr = NULL;
        struct nl_addr* local_addr = NULL;
	sprintf(ns_path, "/proc/%d/ns", pid);

	// open the namespace and setns
	/*
	if (pid != getpid()) {
		ns_fd = open(ns_path, O_RDONLY);
		if (ns_fd == -1) {
			pr_err("can't open ns directory for (%d): %s\n", pid, strerror(errno));
			return -1;
		}

		if (setns(ns_fd, CLONE_NEWNET) < 0) {
			pr_err("error in setns for (%d): %s\n", pid, strerror(errno)); 
			goto cleanup;
		}
	} */

	// open the socket and connect
	sock = nl_socket_alloc();
	if (sock == NULL) {
		pr_err("error in allocating nl_sock\n");
		goto cleanup;
	}

	if (nl_connect(sock, NETLINK_ROUTE) < 0) {
                pr_err("Failed to connect to netlink socket.\n");
		goto cleanup;
        }
	
	// find the link
	if (rtnl_link_alloc_cache(sock, AF_UNSPEC, &cache) < 0) {
		pr_err("error in allocating nl_cache\n");
		goto cleanup;
	}

	int idx = rtnl_link_name2i(cache, if_name);
	if (idx == 0) {
		pr_err("error in rtnl_link_name2i for (%d)\n", pid);
		goto cleanup;
	}

	// Set IP address
        addr = rtnl_addr_alloc();
        rtnl_addr_set_ifindex(addr, idx);
        nl_addr_parse(ip_address, AF_INET, &local_addr);
        rtnl_addr_set_local(addr, local_addr);

        if ((ret = rtnl_addr_add(sock, addr, 0)) < 0) {
                pr_err("Failed to add IP address %s to %s: %s\n", ip_address,
                    if_name, nl_geterror(ret));
                goto cleanup;
        }
	pr_info("IP address %s set to %s\n", ip_address, if_name);

        link = rtnl_link_alloc();

        // Set link up (IFF_UP)
        rtnl_link_set_flags(link, IFF_UP);
        rtnl_link_set_ifindex(link, idx);
        if ((ret = rtnl_link_change(sock, link, link, 0)) < 0) {
                pr_err(
                    "Failed to bring up %s: %s\n", if_name, nl_geterror(ret));
                goto cleanup;
        }
	pr_info("%s is up\n", if_name);

cleanup:
	if (ns_fd != -1)
		close(ns_fd);
	if (sock)
		nl_socket_free(sock);
        if (cache)
                nl_cache_free(cache);
        if (link)
                rtnl_link_put(link);
        if (addr)
                rtnl_addr_put(addr);
        if (local_addr)
                nl_addr_put(local_addr);
        return -1;
}

int create_veth_pair(container_ctx* ctx)
{
        int ret = -1;
        struct nl_sock* sock;
        struct rtnl_link *link = NULL, *peer = NULL;
        int err;

        // Allocate and connect netlink socket
        sock = nl_socket_alloc();
        if (sock == NULL) {
                pr_err("Failed to allocate netlink socket.\n");
                return -1;
        }

        if (nl_connect(sock, NETLINK_ROUTE) < 0) {
                pr_err("Failed to connect to netlink socket.\n");
		goto sock_free;
        }

        // Allocate veth link and set the names
        link = rtnl_link_veth_alloc();
        if (link == NULL) {
                pr_err("Failed to allocate veth link.\n");
                goto sock_free;
        }
        rtnl_link_set_name(link, "veth0");

        // Allocate and set peer information
        peer = rtnl_link_veth_get_peer(link);
        if (peer == NULL) {
                pr_err("Failed to allocate peer.\n");
                goto link_free;
        }

        rtnl_link_set_name(peer, "veth1");
        rtnl_link_set_ns_pid(peer, ctx->child_pid);

        // Send request to the kernel
        if ((err = rtnl_link_add(sock, link, NLM_F_CREATE | NLM_F_REPLACE)) < 0) {
                fprintf(stderr, "Failed to create veth interface: %s\n",
                    nl_geterror(err));
                goto link_free;
        }

        pr_info("veth pair created\n");
        ret = 0;

link_free:
        if (peer)
                rtnl_link_put(peer);
        if (link)
                rtnl_link_put(link);
sock_free:
	if (sock)
		nl_socket_free(sock);
        return ret;
}

int setup_network(container_ctx *ctx) {
	if (create_veth_pair(ctx) < 0) {
		return -1;
	}

	if (configure_interface(ctx->parent_pid, "veth0", "192.168.1.1/24") < 0) {
		pr_err("error in setting up IP address for veth0\n");
		return -1;
	}

	if (configure_interface(ctx->child_pid, "veth1", "192.168.1.2/24") < 0) {
		pr_err("error in setting up IP address for veth1\n");
		return -1;
	}

	return 0;
}
