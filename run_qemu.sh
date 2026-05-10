#!/bin/sh

./env/qemu-11.0.0/build/qemu-system-aarch64 \
    -M virt \
    -cpu host -enable-kvm \
    -kernel env/linux-7.0.3/arch/arm64/boot/Image \
    -initrd env/rootfs.cpio.gz \
    -append "console=ttyAMA0 root=/dev/ram rdinit=/init" \
    -nographic \
    -device vgpu0
