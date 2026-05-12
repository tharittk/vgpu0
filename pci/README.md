### Plan

What are we doing here?
User's space <-> Kernel (Driver) <-> Hardware (QEMU)

# QEMU Hardware <-> Kernel Driver
- Here the hardware is QEMU-emulated PCI device. The hardware also has its own memory, says for IO and its internal data storage.
- In QEMU's device, we hard-code these hardware address to the device (`vpgu0-dev.c`)
- BAR (base-address register ~ hardware address). There are the actual memory says, `uint8 bar0 [64]` and the `MemoryRegion` `mmio_bar0. 
- the QEMU device registers this `mmio_bar0` alongs with its `MemoryRegionOps` callbacks. When every the `mmio_bar0` is accessed (via driver), callback will be triggered.
- those callback interacts with the underlying data `bar0`


# Kernel Driver <-> User's space
- the driver claims the `MemoryRegion` we register above and assign to, says, internal state of the driver
- We can see that now the driver can interact with the user, for example, with char device: let the user writes to it and call the `iowrite` functions to the claimed address.
- Effectively, the hardware detail is abstracted from the user.
- The driver has its own set of callbacks (for char dev, in this case) too. When the user `echo` to the device, that callback is triggered which, again, may call `iowrite` or `ioread`.


# Misc.
- `DECLARE_INSTANCE_CHECKER ()`: define a macro for somewhat ugly casting checker.
  It checks that the pointer to be cased has the expected type (our device).

- platform driver vs pci driver
  . platform driver like those for SoC peripheral is not discoverable during the boot time. Use the device tree binding concept.
  . pci driver are discoverable by matching (vendor id, device id)

- `container_of` trick is used widely here: getting (out) to the enclosing `struct`.
