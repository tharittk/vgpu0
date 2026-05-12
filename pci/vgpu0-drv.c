/* driver for vgpu0 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pci.h>

#define PCI_VENDOR_ID_VGPU0 0xdead
#define PCI_DEVICE_ID_VGPU0 0xbeef

typedef struct vgpu0State{
	struct pci_dev *pdev;
	void __iomem *hw_bar0;
	struct cdev cdev;
} vgpu0State;

/*
 * I/O access via char device
 */


static int vgpu0_open(struct inode *inode, struct file *filp)
{
	vgpu0State *p = container_of(inode->i_cdev, struct vgpu0State, cdev);
	filp->private_data = p;
	return 0;
}
static ssize_t vgpu0_write(struct file *filp, const char __user *user_buffer, size_t count, loff_t *offs)
{
	char *buf;
	int not_copied;
	vgpu0State* state = (vgpu0State *) filp->private_data;

        buf = kmalloc(count, GFP_ATOMIC);
        not_copied = copy_from_user(buf, user_buffer, count); 

	for (size_t i = 0; i < count; i++){
		iowrite8(buf[i], state->hw_bar0 + i);
	}
	return count - not_copied;
}

static ssize_t vgpu0_read(struct file *filp, char __user *user_buffer, size_t count, loff_t *offs)
{
	char *buf;
	int not_copied;
	vgpu0State* state = (vgpu0State *) filp->private_data;

        buf = kmalloc(count, GFP_ATOMIC);
	for (size_t i = 0; i < count; i++){
		u8 val = ioread8(state->hw_bar0 + i);
		buf[i] = val;
	}

        not_copied = copy_to_user(user_buffer, buf, count); 
	return count - not_copied;
}

static struct file_operations fops = {
	.open = vgpu0_open,
	.read = vgpu0_read,
	.write = vgpu0_write
};

static int setup_cdev(vgpu0State *state)
{
	dev_t dev;
	int status;
	status = alloc_chrdev_region(&dev, 0, 1, "vgpu0");
	if (status){
		pr_err("vgpu0 - alloc chrdev\n");
		return status;
	}

	cdev_init(&state->cdev, &fops);
	state->cdev.owner = THIS_MODULE;

	cdev_add(&state->cdev, dev, 1);

	struct class *vgpu0_class = class_create("vgpu0");
	device_create(vgpu0_class, NULL, dev, NULL, "vgpu0%d", 0);
	pr_info("vgpu0 - create dev node dev: %d:%d\n", MAJOR(dev), MINOR(dev));
	return 0;
}

static int vgpu0_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct vgpu0State *state;
	int status;

	state = devm_kzalloc(&pdev->dev, sizeof(struct vgpu0State), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	status = pcim_enable_device(pdev);
	if (status){
		pr_err("vgpu0 - Error enable pci device\n");
		return status;
	}

	state->pdev = pdev;
	status = pci_request_region(pdev, 0, "vgpu0");
	if (status){
		pr_err("vgpu0 - Error claiming mmregion\n");
		return status;
	}

	state->hw_bar0 = pcim_iomap(pdev, 0, pci_resource_len(pdev, 0));
	if(!state->hw_bar0) {
		pr_err("vgpu0 - Error mapping hw_bar0\n");
		status = -ENODEV;
		return status;
	}

	pci_set_drvdata(state->pdev, state);
	dev_info(&pdev->dev, "vgpu0 - Probe ok.\n");
	status = setup_cdev(state);
	pci_set_master(state->pdev);
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
