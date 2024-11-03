# Low Level Container Runtime

## How to run

1. Build the program using gcc (this requires libnl and libcap, so make sure you have these libraries):

```shell
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
```shell
$ sudo ./container --rootfs <root_fs_path> --memory <memory_limit> -- <command> [command_args]

# Example (from the previous step of ubuntu image):
$ sudo ./container --rootfs ./rootfs --memory 1G -- /bin/bash
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

My in-depth notes on `cgroups`, `namespaces` and other container topics are available at : https://www.sheharyaar.in/notes/linux-containers/

