# Low Level Container Runtime

## How to run

1. Build the program using gcc (this requires libnl and libcap, so make sure you have these libraries):

```
$> make
```

2. Download a minimal/base ubuntu root filesystem from https://cdimage.ubuntu.com/ubuntu-base/releases/22.04.4/release/.

3. Extract the rootfs
```shell
# the rootfs will be here
$ mkdir rootfs
$ tar xzvf <tar file> -C rootfs
```

3. Run the program as root :
```
#> ./container --rootfs <root_fs_path> --memory <memory_limit> -- <command> [command_args]

Example (from the previous step of ubuntu image):
#> ./container --rootfs ./rootfs --memory 1G -- /bin/bash
```

Remember, the rootfs cannot be on the current mounted root, `pivot_root` will give out error.

### Status

- [X] UID and GID mapping
- [X] clone setup
- [X] filesystem Setup
- [X] cgroup limits setup
- [ ] network setup (to be done after I study more about kernel networking and netlink, or as part of CNI implementation in near future)

## Playground

- `playground/` has small standalone programs that I used for practising. 

To build the program run `gcc container.c -o container -lcap` <br />
To run the program you need `CAP_SYS_ADMIN`, so you can use sudo: `sudo ./container /bin/bash`

## Notes

- For a basic container `cpuacct`/`cpu`, `memory` and `devices` (eBPF based for cgroups v2) cgroup controllers are only needed.

### Notion Notes

My notes are available as Notion web-sites, they will be updated as I understand these technologies better :

- [Namespaces](https://spark-root-5d4.notion.site/Namespaces-11d47299d3a48059999aff7c7b12037f?pvs=74) | Notion
- [CGroups](https://spark-root-5d4.notion.site/Control-Groups-11d47299d3a480fb8bebcacf1dc78bd9) | Notion

### Steps to create a pair of namespace and connect them

This is also available at my gist : https://gist.github.com/sheharyaar/5cfaa933a8483080c9f1b2129a6135c3#file-network-namespace-md


1. Create the namespaces using `sudo ip netns add <ns_name>`

```bash
sudo ip netns add ns1
sudo ip netns add ns2
```

2. Connect these namespaces using a virtual ethernet (created in pairs) using `sudo ip link add <if_name> netns <ns_name> type veth peer name <ns_name_other>` :

```bash
sudo ip link add veth0 netns ns1 type veth peer name ns2
```

This will create veth0 in `ns1` and veth1 in `ns2`.

3. Assign IP addresses to these veth pairs using `sudo ip -n <ns_name> addr add <ip/mask> dev <if_name> :

```bash
sudo ip -n ns1 addr add 10.1.1.1/24 dev veth0
sudo ip -n ns2 addr add 10.1.1.2/24 dev veth1
```

4. `UP` the interfaces using `sudo ip -n <ns_name> link set <if_name> up` :

```bash
sudo ip -n ns1 set veth0 up
sudo ip -n ns2 set veth1 up
```

5. Exec into the namespaces in different terminals and ping each other :

```bash
sudo ip netns exec ns1 ping 10.1.1.2    # in one terminal
sudo ip netns exec ns2 ping 10.1.1.1    # in another terminal
```

You can also exec `bash` in any namespace and execute commands interactively in that namespace : 

```bash
sudo ip netns exec ns1 /bin/bash
# you will get a terminal now
```
