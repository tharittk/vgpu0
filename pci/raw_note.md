### PCI
- why custom pci ? API for GPU to render on virtual machine.
- build it in QEMU
- wiki.osdev PCI
Ref.
QEMU
- ivshmem (not the one we want). limited capability.
    file mmap for device
- docs/devel/qom.rst 
    .object model, runtime device generation?
    .start with example in here
    . meson.build, Kconfig : you can grep device name (IVSHMEM_DEVICE) and see
which file uses this env to learn about the build
- hw/net/rtl8139.c
    class_init() - shared between all instances
- first dirty my pci device (qemu)
    .writing new device, the most logical path is to follow the existing.
    .copy includes from the existing device and run with it first.
    .first step proof: compile, run with qemu, lspci should show our device
    .first step proof: compile, run with qemu, lspci should show our device
    .<device name>State
    .interface has to be defined.
    .set vendor id, device id to be yours (so you can proof).
    .beware of a lot of casting
- device driver (linux kernel)
    . driver/leds <- by functionality
    . driver/net/ethernet/realtek/8139cp.c
    . i am a pci driver, then on init a bunch of macro ... binary injected. 
    . go by example <-> read doc.
    . driver has a table (vendor id, device id) that tries to instantiate.
    . the probe function.
    . who would iterate id table ? (if the driver is for multiple devices)
    . `__pci_device_probe()` <- already knows the device. 
    . `pci_bus_match` , `driver_attch` <- find which driver matches the device ?
    . drivers/led/Makefile, Kconfig
    . make menuconfig should show our device driver
    . #error at the header helps make sure that our file is hooked to the build
system
- base address region/registers
    . io or mem address
    . datasheet of the pci device
    . you flag whether you are the memory or io
    . this kind of thing, its spec must match the datasheet.
    . <in qemu>
    . actual work is done in realize()
    . MemoryRegion, iops : Linux memory modelled as graph.
    . `memory_region_init_io()` and then `pci_register_bar`
    . MemoryRegionOps, write=write_callback
    . opaque pointer convert to our device.
    . add realize callback to device realize (previously only vendor id and
num).

    . try writing something to this BAR (back to the driver)
    . when making a simple version of ourself from existing, keep asking: do i need this?
    . if you don't know, try leaving that OUT (so when thing does not work, you
come back and knows better why).
    . devm_kzalloc is better hoice than kzalloc (managed memmory with lifetime
of the device)
    . since we didn't do kernel module, rebuild the kernel
    . try write something after probe (testing trigger the memops)
    . add .write and .read
- hook up to LED subsystem (led.h, led-blinkm.c)
    . user container_of style.
    . in the yourdev_priv, add led_classdev inside
    . leds-class (/sys/class/leds)
    . rgb light should then be 3 led_classdev
    . each has a callback on .brightness_set blocking=
    . led brightness (enum takes only 0, 1, 127, 255) ?
    . need to register: led_classdev_register
    . try run QEMU, see if /sys/class/leds/ has your files of R, G, B
- how to check that led works?
    . write led rgb to file and display on webserver
    . fopen, fprintf, file truncation aspect
    . serving .svg (just open it with firefox/web browser)
    . fprintf .svg instead of json
    . html page that refreshes (serving .svg)
    












