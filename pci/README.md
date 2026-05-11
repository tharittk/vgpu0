### Plan

# QEMU
- build the vpci0.c which will be compiled in the qemu/hw/misc
- show that once we launch with qemu -device vpci0, lspci detects it

# Driver
- put our functionality as the led (`driver/leds/`)
- initiate the memoryregion (io or mem address)
- proof that write callback and read callback works for these memory region.
- hook this up to the led subsystem. if we echo to the led brightness, it should
  update the svg/json file.

# Implementation
- DECLARE_INSTANCE_CHECKER (): define a macro for somewhat ugly casting checker.
  It checks that the pointer to be cased has the expected type (our device).
(our device).


- platform driver vs pci driver
  . platform driver like those for SoC peripheral is not discoverable during the boot time. Use the device tree binding concept.

  . pci driver are discoverable by matching (vendor id, device id)
