### Setting up the environemnt

We need 3 things: Linux Kernel, BusyBox, and QEMU.

# Kernel
Get the stable linux kernel version

`$ wget https://cdn.kernel.org/pub/linux/kernel/v7.x/linux-7.0.5.tar.xz`
On my Raspberry Pi 5

```
$ make defconfig
$ make -j $(nproc)
```

Note that we don't do `$ make bcm2712_defconfig` - which is the optimal build for
Raspberry Pi 5 -  because we will emulate the with qemu i.e., we want the
generic kernel.

# BusyBox
[reference](https://lukaszgemborowski.github.io/articles/minimalistic-linux-system-on-qemu-arm.html)

We will go easy with building the `initrd`. We will build the root filesystem
compress it to `.cpio` and uncomrpessed that during the kernel boot

`$ wget https://busybox.net/downloads/busybox-1.36.1.tar.bz2`

```
$ tar -xvf busybox-1.36.1.tar.bz2
$ cd busybox-1.36.1`
```
We will build the with the static linking here. We want the root filesystem to
be self-contatined. Here we don't worry so much about binary size.

```
$ make defconfig
$ make menuconfig  # Enable Static Binary here
$ make -j$(nproc)
$ make install
```
We also need the `init` script for the kernel to run as first process.

```
$ cd ..
$ mkdir -p rootfs
$ vim init
```
the init file include
```bash
#!/bin/sh

mount -t proc none /proc
mount -t sysfs none /sys
mknod -m 660 /dev/mem c 1 1

exec /bin/sh
```
Make it executable:

`$ chmod +x rootfs/init`

Copy busybox artifacts to root filesystem:

`$ cp -av busybox-1.36.1/_install/* rootfs/`
 
We are still missing the standard directory layout.

`$ mkdir -pv rootfs/{bin,sbin,etc,proc,sys,usr/{bin,sbin}}`

Last step is to compress it to the ramdisk
```
$ cd rootfs
$ find . -print0 | cpio --null -ov --format=newc | gzip -9 > ../rootfs.cpio.gz
```

# QEMU



