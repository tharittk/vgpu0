/* pci device for vgpu0 */
#include "qemu/osdep.h"
#include "hw/pci/pci_device.h"
#include "hw/core/qdev-properties.h"
#include "qemu/module.h"
#include "qom/object.h"
#include "system/system.h"

#define TYPE_PCI_VGPU0_DEVICE "vgpu0"
#define PCI_VENDOR_ID_VGPU0 0xdead
#define PCI_DEVICE_ID_VGPU0 0xbeef

typedef struct vgpu0State vgpu0State;
struct vgpu0State {
	PCIDevice pdev;
    MemoryRegion mmio_bar0;
    uint8_t bar0[64];
};

DECLARE_INSTANCE_CHECKER(vgpu0State, VGPU0, TYPE_PCI_VGPU0_DEVICE)

static uint64_t vgpu0_io_read (void *opaque, hwaddr addr, unsigned size)
{
    vgpu0State *state = (vgpu0State *) opaque;
    uint64_t val = state->bar0[addr >> 2];
    printf("vgpu0 - io_read: 0x%lx\n", val);

    return val;
}

static void vgpu0_io_write (void *opaque, hwaddr addr, uint64_t val, unsigned size){
    vgpu0State *state = (vgpu0State *) opaque;
    * (uint64_t*)&state->bar0[addr>>2] = val;
    printf("vgpu0 - io_write 0x%lx to %p\n", val, &state->bar0[addr>>2]);
    return;
}


static const MemoryRegionOps vgpu0_io_ops = {
    .read = vgpu0_io_read,
    .write = vgpu0_io_write,
};

static void vgpu0_realize(PCIDevice *pdev, Error **errp)
{
	vgpu0State *s = VGPU0(pdev);
    //DeviceState *d = DEVICE(pdev);

    memory_region_init_io(&s->mmio_bar0, OBJECT(s), &vgpu0_io_ops, s, "vgpu0-io", 0x40);

    pci_register_bar(pdev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->mmio_bar0);

	printf("vgpu0 - realized with iosize: 0x%x\n", (unsigned int)s->mmio_bar0.size);
}

static void vgpu0_uninit(PCIDevice *pdev)
{
	printf("VGPU0 - uninit!\n");
}

static void vgpu0_class_init(ObjectClass *klass, const void *data)
{

	DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->realize = vgpu0_realize;
    k->exit = vgpu0_uninit;

    k->vendor_id = PCI_VENDOR_ID_VGPU0;
    k->device_id = PCI_DEVICE_ID_VGPU0;

	k->class_id = PCI_CLASS_OTHERS;
	set_bit(DEVICE_CATEGORY_MISC, dc->categories);
}

static void vgpu0_instance_init(Object *obj)
{
	printf("vgpu0 - instance_init!\n");
}

static const TypeInfo vgpu0_info = {
    .name	   = TYPE_PCI_VGPU0_DEVICE,
    .parent	   = TYPE_PCI_DEVICE,
    .instance_size = sizeof(vgpu0State),
    .class_init    = vgpu0_class_init,
    .instance_init = vgpu0_instance_init,
    .interfaces = (const InterfaceInfo[]) {
	{ INTERFACE_CONVENTIONAL_PCI_DEVICE },
	{ },
    },
};

static void vgpu0_register_types(void)
{
    type_register_static(&vgpu0_info);
}

type_init(vgpu0_register_types);

