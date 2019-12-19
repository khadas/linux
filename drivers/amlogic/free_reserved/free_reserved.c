// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/mm.h>
#include <linux/printk.h>
#include <linux/string.h>

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/notifier.h>

#define FREEMEM_DEV_NAME "ac_freemem"

#define RES_IOC_MAGIC  'R'
#define RES_IOCTL_CMD_FREE   _IO(RES_IOC_MAGIC, 0x00)

static struct reserved_mem res_mem;
static bool free_memory;
static dev_t dev_no;

struct aml_free_dev {
	struct platform_device *pdev;
	struct class cls;
};

#define AML_FREE_NAME		"free_reserved"

static int __init sample_res_setup(struct reserved_mem *rmem)
{
	phys_addr_t align = PAGE_SIZE;
	phys_addr_t mask = align - 1;

	if ((rmem->base & mask) || (rmem->size & mask)) {
		pr_err("Reserved memory: incorrect alignment of region\n");
		return -EINVAL;
	}
	res_mem.base = rmem->base;
	res_mem.size = rmem->size;
	/*rmem->ops = &rmem_fb_ops;*/
	pr_info("Reserved memory: created fb at 0x%pa, size %ld MiB\n",
		&rmem->base, (unsigned long)rmem->size / SZ_1M);
	return 0;
}

RESERVEDMEM_OF_DECLARE(sample_reserved,
		       "amlogic, aml_autocap_memory",
		       sample_res_setup);

#ifdef CONFIG_64BIT
static void free_reserved_highmem(unsigned long start, unsigned long end)
{
}
#else
static void free_reserved_highmem(unsigned long start, unsigned long end)
{
	for (; start < end; ) {
		free_highmem_page(phys_to_page(start));
	start += PAGE_SIZE;
	}
}
#endif

static void free_reserved_mem(unsigned long start, unsigned long size)
{
	unsigned long end = PAGE_ALIGN(start + size);
	struct page *page, *epage;

	page = phys_to_page(start);
	if (PageHighMem(page)) {
		free_reserved_highmem(start, end);
	} else {
		epage = phys_to_page(end);
		if (!PageHighMem(epage)) {
			free_reserved_area(__va(start),
					   __va(end), 0, "free_reserved");
		} else {
			/* reserved area cross zone */
			struct zone *zone;
			unsigned long bound;

			zone  = page_zone(page);
			bound = zone_end_pfn(zone);
			free_reserved_area(__va(start),
					   __va(bound << PAGE_SHIFT),
					   0, "free_reserved");
			zone  = page_zone(epage);
			bound = zone->zone_start_pfn;
			free_reserved_highmem(bound << PAGE_SHIFT, end);
		}
	}
}

static ssize_t free_reserved_show(struct class *cls,
				  struct class_attribute *attr, char *buf)
{
	if (free_memory == 1)
		return 0;

	free_reserved_mem(res_mem.base, res_mem.size);

	free_memory = 1;
	return 0;
}

static ssize_t free_reserved_store(struct class *cls,
				   struct class_attribute *attr,
	       const char *buffer, size_t count)
{
	if (free_memory == 1)
		pr_info("Memory is freed at 0x%pa, size %ld MiB\n",
			&res_mem.base, (unsigned long)res_mem.size / SZ_1M);
	else
		pr_info("Memory is reserved at 0x%pa, size %ld MiB\n",
			&res_mem.base, (unsigned long)res_mem.size / SZ_1M);

	return count;
}

static CLASS_ATTR_RW(free_reserved);

static struct attribute *aml_free_attrs[] = {
	&class_attr_free_reserved.attr,
	NULL
};

ATTRIBUTE_GROUPS(aml_free);

static int free_mem_mmap(struct file *file, struct vm_area_struct *vma)
{
	int ret = 0;

	if (vma->vm_end - vma->vm_start > res_mem.size) {
		pr_err("vma size is larger than reserved memory\n");
		return -EINVAL;
	}

	ret = remap_pfn_range(vma, vma->vm_start,
			      res_mem.base >> PAGE_SHIFT,
			      vma->vm_end - vma->vm_start, vma->vm_page_prot);
	if (ret != 0) {
		pr_err("Failed to mmap reserved memory\n");
		return -EINVAL;
	}

	return 0;
}

static long free_mem_ioctl(struct file *file, unsigned int cmd,
			   unsigned long arg)
{
	switch (cmd) {
	case RES_IOCTL_CMD_FREE:{
		if (free_memory == 1)
			return 0;
		free_reserved_mem(res_mem.base, res_mem.size);
		free_memory = 1;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

#ifdef CONFIG_COMPAT
static long free_mem_compat_ioctl(struct file *file, unsigned int cmd,
				  unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = free_mem_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations free_mem_fops = {
	.owner   = THIS_MODULE,
#ifdef CONFIG_COMPAT
	.compat_ioctl = free_mem_compat_ioctl,
#endif
	.unlocked_ioctl = free_mem_ioctl,
	.mmap    = free_mem_mmap,
};

static int aml_free_probe(struct platform_device *pdev)
{
	struct aml_free_dev *jdev;
	int ret;
	int r;

	/* kzalloc device */
	jdev = devm_kzalloc(&pdev->dev,
			    sizeof(struct aml_free_dev), GFP_KERNEL);
	if (!jdev)
		return -ENOMEM;

	/* set driver data */
	jdev->pdev = pdev;
	platform_set_drvdata(pdev, jdev);

	/* create class attributes */
	jdev->cls.name = AML_FREE_NAME;
	jdev->cls.owner = THIS_MODULE;
	jdev->cls.class_groups = aml_free_groups;
	ret = class_register(&jdev->cls);
	if (ret) {
		pr_err("couldn't register sysfs class\n");
		return ret;
	}

	r = register_chrdev(0, FREEMEM_DEV_NAME,
			    &free_mem_fops);
	if (r < 0)
		ret = -EPERM;
	dev_no = r;
	device_create(&jdev->cls, &pdev->dev,
		      MKDEV(dev_no, 0),
		      NULL, FREEMEM_DEV_NAME);
	return ret;
}

static int __exit aml_free_remove(struct platform_device *pdev)
{
	struct aml_free_dev *jdev = platform_get_drvdata(pdev);

	class_unregister(&jdev->cls);
	platform_set_drvdata(pdev, NULL);
	unregister_chrdev(dev_no, FREEMEM_DEV_NAME);

	return 0;
}

static const struct of_device_id aml_free_reserved_dt_match[] = {
	{
		.compatible = "amlogic, free_reserved",
	},
	{}
};

static struct platform_driver aml_free_driver = {
	.driver = {
		.name = AML_FREE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = aml_free_reserved_dt_match,
	},
	.probe = aml_free_probe,
	.remove = __exit_p(aml_free_remove),
};

static int __init aml_free_reserved_init(void)
{
	if (platform_driver_register(&aml_free_driver)) {
		pr_err("failed to register driver\n");
		return -ENODEV;
	}

	return 0;
}

fs_initcall(aml_free_reserved_init);

static void __exit aml_free_reserved_exit(void)
{
	platform_driver_unregister(&aml_free_driver);
}

module_exit(aml_free_reserved_exit);

MODULE_DESCRIPTION("Free reserved memory");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Amlogic, Inc.");
