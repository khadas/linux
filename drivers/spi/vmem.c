// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/amlogic/vmem.h>

#define TEST_VMEM_ENTRY

static LIST_HEAD(vmem_controller_list);

struct vmem_controller *vmem_get_controller(int bus_num)
{
	struct vmem_controller *vmemctlr;

	list_for_each_entry(vmemctlr, &vmem_controller_list, list)
		if (vmemctlr && vmemctlr->bus_num == bus_num)
			return vmemctlr;

	return NULL;
}

struct vmem_device *
vmem_get_device(struct vmem_controller *vmemctlr, int dev_num)
{
	struct vmem_device *vmemdev;

	list_for_each_entry(vmemdev, &vmemctlr->device_list, list)
		if (vmemdev && vmemdev->dev_num == dev_num)
			return vmemdev;

	return NULL;
}

struct vmem_controller *vmem_create_controller(struct device *dev, int size)
{
	struct vmem_controller *vmemctlr;
	u32 bus_num;

	if (!dev)
		return NULL;

	if (of_property_read_u32(dev->of_node, "vmem-bus-num", &bus_num))
		bus_num = 0;

	vmemctlr = vmem_get_controller(bus_num);
	if (vmemctlr) {
		dev_err(dev, "vmemctlr created already\n");
		return NULL;
	}

	vmemctlr = kzalloc(sizeof(*vmemctlr) + size, GFP_KERNEL);
	if (!vmemctlr)
		return NULL;

	vmemctlr->dev.parent = dev;
	device_initialize(&vmemctlr->dev);
	dev_set_drvdata(&vmemctlr->dev, &vmemctlr[1]);
	dev_set_name(&vmemctlr->dev, "vmemctlr%u", bus_num);
	if (device_add(&vmemctlr->dev)) {
		kfree(vmemctlr);
		dev_err(dev, "vmemctlr add device failed\n");
		return NULL;
	}

	vmemctlr->bus_num = bus_num;
	list_add_tail(&vmemctlr->list, &vmem_controller_list);
	INIT_LIST_HEAD(&vmemctlr->device_list);
	dev_info(dev, "vmemctlr %s created!\n", dev_name(&vmemctlr->dev));

	return vmemctlr;
}

void vmem_destroy_controller(struct vmem_controller *vmemctlr)
{
	device_del(&vmemctlr->dev);
	list_del(&vmemctlr->list);
	kfree(vmemctlr);
}

struct vmem_device *vmem_create_device(struct vmem_controller *vmemctlr,
				       int dev_num, int mem_size)
{
	struct device *dev;
	struct vmem_device *vmemdev;

	dev = &vmemctlr->dev;
	vmemdev = vmem_get_device(vmemctlr, dev_num);
	if (vmemdev) {
		dev_err(dev, "vmemdev created already\n");
		return NULL;
	}

	vmemdev = kzalloc(sizeof(*vmemdev) + mem_size, GFP_KERNEL);
	if (!vmemdev)
		return NULL;

	vmemdev->dev.parent = dev;
	device_initialize(&vmemdev->dev);
	vmemdev->mem = (u8 *)&vmemdev[1];
	vmemdev->size = mem_size;
	dev_set_name(&vmemdev->dev, "%s.%u", dev_name(dev), dev_num);
	if (device_add(&vmemdev->dev)) {
		kfree(vmemdev);
		dev_err(dev, "vmemdev add device failed\n");
		return NULL;
	}

	vmemdev->vmemctlr = vmemctlr;
	vmemdev->dev_num = dev_num;
	list_add_tail(&vmemdev->list, &vmemctlr->device_list);
	dev_info(dev, "vmemdev %s created!\n", dev_name(&vmemdev->dev));

	return vmemdev;
}

void vmem_destroy_device(struct vmem_device *vmemdev)
{
	device_del(&vmemdev->dev);
	list_del(&vmemdev->list);
	kfree(vmemdev);
}

int vmem_setup(struct vmem_device *vmemdev)
{
	struct vmem_controller *vmemctlr = vmemdev->vmemctlr;
	int ret = 0;

	if (vmemctlr->setup)
		ret = vmemctlr->setup(vmemdev);

	return ret;
}

int vmem_cleanup(struct vmem_device *vmemdev)
{
	struct vmem_controller *vmemctlr = vmemdev->vmemctlr;
	int ret = 0;

	if (vmemctlr->cleanup)
		ret = vmemctlr->cleanup(vmemdev);

	return ret;
}

#ifdef TEST_VMEM_ENTRY
static struct vmem_device *test_vmem;

static int test_vmem_cmd_notify(u8 cmd_val, int offset, int size)
{
	return 0;
}

static int test_vmem_data_notify(u8 cmd_val, int offset, int size)
{
	pr_info("\ncmd 0x%x, offset 0x%x, size 0x%x\n", cmd_val, offset, size);
	return 0;
}

static ssize_t create_store(struct class *class, struct class_attribute *attr,
			    const char *buf, size_t count)
{
	struct vmem_controller *vmemctlr;
	unsigned int bus_num, dev_num, size;
	int i;

	if (test_vmem) {
		pr_info("error: test vmem existing\n");
		return -EINVAL;
	}

	if (sscanf(buf, "%d%d%d", &bus_num, &dev_num, &size) != 3) {
		bus_num = 0;
		dev_num = 0;
		size = 32;
	}
	vmemctlr = vmem_get_controller(bus_num);
	if (!vmemctlr)
		return -ENODEV;

	test_vmem = vmem_create_device(vmemctlr, dev_num, size);
	test_vmem->cmd_notify = test_vmem_cmd_notify;
	test_vmem->data_notify = test_vmem_data_notify;
	for (i = 0; i < size; i++)
		test_vmem->mem[i] = i + 0x80;

	return count;
}

static ssize_t destroy_store(struct class *class, struct class_attribute *attr,
			     const char *buf, size_t count)
{
	if (test_vmem) {
		vmem_destroy_device(test_vmem);
		test_vmem = NULL;
	}
	return count;
}

static ssize_t data_show(struct class *class, struct class_attribute *attr,
			 char *buf)
{
	int i;

	if (test_vmem) {
		for (i = 0; i < test_vmem->size; i++)
			pr_info("mem[%d]= 0x%x\n", i, test_vmem->mem[i]);
	}

	return 0;
}

static CLASS_ATTR_WO(create);
static CLASS_ATTR_WO(destroy);
static CLASS_ATTR_RO(data);

static struct attribute *test_vmem_class_attrs[] = {
	&class_attr_create.attr,
	&class_attr_destroy.attr,
	&class_attr_data.attr,
	NULL,
};

ATTRIBUTE_GROUPS(test_vmem_class);

static struct class test_vmem_class = {
	.name = "test_vmem",
	.owner = THIS_MODULE,
	.class_groups = test_vmem_class_groups
};

static int __init vmem_init(void)
{
	test_vmem = NULL;
	return class_register(&test_vmem_class);
}

postcore_initcall(vmem_init);
#endif

