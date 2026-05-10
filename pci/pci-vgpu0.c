/* pci device for vGPU */
#include "qemu/osdep.h"
#include "hw/pci/pci_device.h"
#include "hw/core/qdev-properties.h"
#include "qemu/module.h"
#include "qom/object.h"

#define TYPE_PCI_VGPU0_DEVICE "vgpu0"
#define PCI_VENDOR_ID_VGPU0 0xdead
#define PCI_DEVICE_ID_VGPU0 0xbeef


typedef struct vgpu0State vgpu0State;
struct vgpu0State {
	PCIDevice pdev;
};

DECLARE_INSTANCE_CHECKER(vgpu0State, VGPU0, TYPE_PCI_VGPU0_DEVICE)

static void vgpu0_realize(PCIDevice *pdev, Error **errp)
{
	//vgpu0State *vgpu0 = VGPU0(pdev);
	printf("VGPU0 - Realized !\n");
}	

static void vgpu0_uninit(PCIDevice *pdev)
{
	printf("VGPU0 - uninit!\n");
}

static void vgpu0_class_init(ObjectClass *klass, const void *data)
{
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->realize = vgpu0_realize;
    k->exit = vgpu0_uninit;

    k->vendor_id = PCI_VENDOR_ID_VGPU0;
    k->device_id = PCI_DEVICE_ID_VGPU0;
}

static void vgpu0_instance_init(Object *obj)
{
	printf("VGPU0 - instance_init!\n");
}

static const TypeInfo vgpu0_info = {
    .name          = TYPE_PCI_VGPU0_DEVICE,
    .parent        = TYPE_PCI_DEVICE,
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

