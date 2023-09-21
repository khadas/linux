// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/pagemap.h>
#include <linux/io.h>

#define DEVICE_NAME "aml-gpiomem"
#define DRIVER_NAME "gpiomem-aml"
#define DEVICE_MINOR 0

struct aml_gpiomem_instance {
	unsigned long gpio_regs_phys;
	struct device *dev;
};

static struct cdev aml_gpiomem_cdev;
static dev_t aml_gpiomem_devid;
static struct class *aml_gpiomem_class;
static struct device *aml_gpiomem_dev;
static struct aml_gpiomem_instance *inst;

/**************************************************************************/
/*   GPIO mem chardev file ops                                            */
/**************************************************************************/

static int aml_gpiomem_open(struct inode *inode, struct file *file)
{
	int dev = iminor(inode);
	int ret = 0;

	if (dev != DEVICE_MINOR) {
		dev_err(inst->dev, "Unknown minor device: %d", dev);
		ret = -ENXIO;
	}
	return ret;
}

static int aml_gpiomem_release(struct inode *inode, struct file *file)
{
	int dev = iminor(inode);
	int ret = 0;

	if (dev != DEVICE_MINOR) {
		dev_err(inst->dev, "Unknown minor device %d", dev);
		ret = -ENXIO;
	}
	return ret;
}

static const struct vm_operations_struct aml_gpiomem_vm_ops = {
#ifdef CONFIG_HAVE_IOREMAP_PROT
	.access = generic_access_phys
#endif
};

static int aml_gpiomem_mmap(struct file *file, struct vm_area_struct *vma)
{
	/* Ignore what the user says - they're getting the GPIO regs */
	/*   whether they like it or not! */
	unsigned long gpio_page = inst->gpio_regs_phys >> PAGE_SHIFT;

	vma->vm_page_prot = phys_mem_access_prot(file, gpio_page,
						 PAGE_SIZE,
						 vma->vm_page_prot);
	vma->vm_ops = &aml_gpiomem_vm_ops;
	if (remap_pfn_range(vma, vma->vm_start,
			gpio_page,
			PAGE_SIZE,
			vma->vm_page_prot)) {
		return -EAGAIN;
	}
	return 0;
}

static const struct file_operations
aml_gpiomem_fops = {
	.owner = THIS_MODULE,
	.open = aml_gpiomem_open,
	.release = aml_gpiomem_release,
	.mmap = aml_gpiomem_mmap,
};

/***************************************************************************/
/*   Probe and remove functions                                            */
/***************************************************************************/

static int aml_gpiomem_probe(struct platform_device *pdev)
{
	int err;
	void *ptr_err;
	struct device *dev = &pdev->dev;
	struct resource *ioresource;

	/* Allocate buffers and instance data */
	inst = kzalloc(sizeof(*inst), GFP_KERNEL);

	if (!inst) {
		err = -ENOMEM;
		goto failed_inst_alloc;
	}

	inst->dev = dev;

	ioresource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (ioresource) {
		inst->gpio_regs_phys = ioresource->start;
	} else {
		dev_err(inst->dev, "failed to get IO resource");
		err = -ENOENT;
		goto failed_get_resource;
	}

	/* Create character device entries */
	err = alloc_chrdev_region(&aml_gpiomem_devid,
				  DEVICE_MINOR, 1, DEVICE_NAME);
	if (err != 0) {
		dev_err(inst->dev, "unable to allocate device number");
		goto failed_alloc_chrdev;
	}
	cdev_init(&aml_gpiomem_cdev, &aml_gpiomem_fops);
	aml_gpiomem_cdev.owner = THIS_MODULE;
	err = cdev_add(&aml_gpiomem_cdev, aml_gpiomem_devid, 1);
	if (err != 0) {
		dev_err(inst->dev, "unable to register device");
		goto failed_cdev_add;
	}

	/* Create sysfs entries */
	aml_gpiomem_class = class_create(THIS_MODULE, DEVICE_NAME);
	ptr_err = aml_gpiomem_class;
	if (IS_ERR(ptr_err))
		goto failed_class_create;

	aml_gpiomem_dev = device_create(aml_gpiomem_class, NULL,
					aml_gpiomem_devid, NULL,
					"gpiomem");
	ptr_err = aml_gpiomem_dev;
	if (IS_ERR(ptr_err))
		goto failed_device_create;

	dev_info(inst->dev, "Initialised: Registers at 0x%08lx",
		inst->gpio_regs_phys);

	return 0;

failed_device_create:
	class_destroy(aml_gpiomem_class);
failed_class_create:
	cdev_del(&aml_gpiomem_cdev);
	err = PTR_ERR(ptr_err);
failed_cdev_add:
	unregister_chrdev_region(aml_gpiomem_devid, 1);
failed_alloc_chrdev:
failed_get_resource:
	kfree(inst);
failed_inst_alloc:
	dev_err(inst->dev, "could not load aml_gpiomem");
	return err;
}

static int aml_gpiomem_remove(struct platform_device *pdev)
{
	struct device *dev = inst->dev;

	kfree(inst);
	device_destroy(aml_gpiomem_class, aml_gpiomem_devid);
	class_destroy(aml_gpiomem_class);
	cdev_del(&aml_gpiomem_cdev);
	unregister_chrdev_region(aml_gpiomem_devid, 1);

	dev_info(dev, "GPIO mem driver removed - OK");
	return 0;
}

/**************************************************************************/
/*   Register the driver with device tree                                 */
/**************************************************************************/

static const struct of_device_id aml_gpiomem_of_match[] = {
	{.compatible = "amlogic, gpiomem",},
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, aml_gpiomem_of_match);

static struct platform_driver aml_gpiomem_driver = {
	.probe = aml_gpiomem_probe,
	.remove = aml_gpiomem_remove,
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = aml_gpiomem_of_match,
	},
};

module_platform_driver(aml_gpiomem_driver);

MODULE_ALIAS("platform:gpiomem-aml");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("gpiomem driver for accessing GPIO from userspace");
MODULE_AUTHOR("Frank Wang <frankk@khadas.com>");
