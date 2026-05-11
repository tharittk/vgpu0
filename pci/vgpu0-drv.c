/* driver for vgpu0 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pci.h>

#define VGPU0_MAX_LEDS				3
#define VGPU0_MAX_BRIGHTNESS			9

#define PCI_VENDOR_ID_VGPU0 0xdead
#define PCI_DEVICE_ID_VGPU0 0xbeef

struct vgpu0State{
	struct device *dev;
	void __iomem *base;
	struct led_classdev cdev;
	/* red, green, blue, alpha */
	u32 rgb;
	bool active_low;
};

/*
 * I/O access via char device
 */

static void vgpu0_write(struct vgpu0State *state);

static unsigned long vgpu0_read(struct vgpu0State *state);

/*
 * API callbacks
 */

static void vgpu0States_brightness_set(struct led_classdev *led_cdev,
					 enum led_brightness value);


static int vgpu0_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct vgpu0State *state;
	int ret;
	ret = pcim_enable_device(pdev);
	if (ret)
		return ret;

	state = devm_kzalloc(&pdev->dev, sizeof(struct vgpu0State), GFP_KERNEL);
	if (!state)
		return -ENOMEM;
	dev_info(&pdev->dev, "vgpu0 - Probe ok.\n");
	return 0;
}

static const struct pci_device_id vgpu0_pci_ids[] = {
    { PCI_DEVICE(PCI_VENDOR_ID_VGPU0, PCI_DEVICE_ID_VGPU0) },
    {}
};

MODULE_DEVICE_TABLE(pci, vgpu0_pci_ids);

static struct pci_driver vgpu0_driver = {
	.name = "vgpu0",
	.probe = vgpu0_probe,
	.id_table = vgpu0_pci_ids,
};

module_pci_driver(vgpu0_driver);

MODULE_AUTHOR("Tharit T.");
MODULE_DESCRIPTION("VGPU0 driver for self-education");
MODULE_LICENSE("GPL");
