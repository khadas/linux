/*
 * drivers/amlogic/amlnf/ntd/aml_ntdcore.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ptrace.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/major.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/ioctl.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/idr.h>
#include <linux/backing-dev.h>
#include <linux/gfp.h>
#include <linux/pm.h>

#include "aml_ntd.h"

/*
 * backing device capabilities for non-mappable devices (such as NAND flash)
 * - permits private mappings, copies are taken of the data
 */
/* static struct backing_dev_info ntd_bdi_unmappable = { */
/* .capabilities	= BDI_CAP_MAP_COPY, */
/* }; */

static int ntd_cls_suspend(struct device *dev, pm_message_t state);
static int ntd_cls_resume(struct device *dev);
static int init_ntd(void);
static DEFINE_SPINLOCK(ntd_idr_lock);
static atomic_t ntd_init_available = ATOMIC_INIT(1);

static struct class ntd_class = {
	.name = "ntd",
	.owner = THIS_MODULE,
	.suspend = ntd_cls_suspend,
	.resume = ntd_cls_resume,
};

static DEFINE_IDR(ntd_idr);

/* These are exported solely for the purpose of ntd_blkdevs.c. You
   should not use them for _anything_ else */
DEFINE_MUTEX(ntd_table_mutex);
EXPORT_SYMBOL(ntd_table_mutex);

struct ntd_info *__ntd_next_device(int i)
{
	return idr_get_next(&ntd_idr, &i);
}
EXPORT_SYMBOL_GPL(__ntd_next_device);


#define NTD_CHAR_MAJOR  222
#define NTD_DEVT(index) MKDEV(NTD_CHAR_MAJOR, (index)*2)


/*****************************************************************************
*Name         :
*Description  :
 * REVISIT once NTD uses the driver model better, whoever allocates
 * the ntd_info will probably want to use the release() hook...
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static void ntd_release(struct device *dev)
{
	dev_t index = NTD_DEVT(dev_to_ntd(dev)->index);

	/* remove /dev/ntdXro node if needed */
	if (index)
		device_destroy(&ntd_class, index + 1);
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static int ntd_cls_suspend(struct device *dev, pm_message_t state)
{
	struct ntd_info *ntd = dev_to_ntd(dev);

	if (ntd && ntd->suspend) {
		aml_nand_msg("ntd_cls_suspend %s", ntd->name);
		return ntd->suspend(ntd);
	} else {
		aml_nand_msg("ntd_cls_suspend null");
		return 0;
	}
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static int ntd_cls_resume(struct device *dev)
{
	struct ntd_info *ntd = dev_to_ntd(dev);

	if (ntd && ntd->resume) {
		aml_nand_msg("ntd_cls_resume %s", ntd->name);
		ntd->resume(ntd);
	} else
		aml_nand_msg("ntd_cls_resume null");

	return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static ssize_t ntd_flags_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct ntd_info *ntd = dev_to_ntd(dev);

	return snprintf(buf, PAGE_SIZE, "0x%lx\n", (unsigned long)ntd->flags);

}
static DEVICE_ATTR(flags, S_IRUGO, ntd_flags_show, NULL);

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static ssize_t ntd_size_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct ntd_info *ntd = dev_to_ntd(dev);

	return snprintf(buf,
		PAGE_SIZE,
		"%llu\n",
		(unsigned long long)ntd->size);

}
static DEVICE_ATTR(size, S_IRUGO, ntd_size_show, NULL);

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static ssize_t ntd_name_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct ntd_info *ntd = dev_to_ntd(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n", ntd->name);

}
static DEVICE_ATTR(name, S_IRUGO, ntd_name_show, NULL);

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static struct attribute *ntd_attrs[] = {
	/* &dev_attr_type.attr, */
	&dev_attr_flags.attr,
	&dev_attr_size.attr,
	/* &dev_attr_blocksize.attr, */
	/* &dev_attr_pagesize.attr, */
	/* &dev_attr_subpagesize.attr, */
	/* &dev_attr_oobsize.attr, */
	/* &dev_attr_numeraseregions.attr, */
	&dev_attr_name.attr,
	NULL,
};

static struct attribute_group ntd_group = {
	.attrs = ntd_attrs,
};

static const struct attribute_group *ntd_groups[] = {
	&ntd_group,
	NULL,
};

static struct device_type ntd_devtype = {
	.name = "ntd",
	.groups = ntd_groups,
	.release = ntd_release,
};

/*****************************************************************************
*Name         :add_ntd_device
*Description  :register an NTD device
*Parameter    :@ntd: pointer to new NTD device info structure
*Return       :
*Note         :
 *	Add a device to the list of NTD devices present in the system, and
 *	notify each currently active NTD 'user' of its arrival. Returns
 *	zero on success or 1 on failure, which currently will only happen
 *	if there is insufficient memory or a sysfs error.
*****************************************************************************/
int add_ntd_device(struct ntd_info *ntd)
{
	int i, error;

	/*
	if (!ntd->backing_dev_info) {
		ntd->backing_dev_info = &ntd_bdi_unmappable;
	}
	*/

	BUG_ON(ntd->pagesize == 0);

	mutex_lock(&ntd_table_mutex);
	/*
	do {
		if (!idr_pre_get(&ntd_idr, GFP_KERNEL))
			goto fail_locked;
		error = idr_get_new(&ntd_idr, ntd, &i);
	} while (error == -EAGAIN);
	*/
	idr_preload(GFP_KERNEL);
	spin_lock_irq(&ntd_idr_lock);
	i = idr_alloc(&ntd_idr, ntd, 0, 0, GFP_NOWAIT);
	spin_unlock_irq(&ntd_idr_lock);
	idr_preload_end();
	if (i < 0)
		goto fail_locked;

	ntd->index = i;
	ntd->usecount = 0;

	/* Some chips always power up locked. Unlock them now */
	/*
	if ((ntd->flags & NTD_WRITEABLE) && (ntd->flags & NTD_POWERUP_LOCK)) {
		if (ntd->unlock(ntd, 0, ntd->blocksize))
			printk(KERN_WARNING"%s: unlock failed,
			writes may not work\n",ntd->name);
	}
	*/
	if (!atomic_dec_and_test(&ntd_init_available))
		atomic_inc(&ntd_init_available);
	else {
		error = init_ntd();
		if (error != 0)
			goto fail_locked;
	}

	/* Caller should have set dev.parent to match the physical device.*/
	ntd->dev.class = &ntd_class;
	ntd->dev.devt = NTD_DEVT(i);
	ntd->dev.parent = &platform_bus;
	ntd->dev.type = &ntd_devtype;

	dev_set_name(&ntd->dev, "ntd%d", i);
	dev_set_drvdata(&ntd->dev, ntd);
	error = device_register(&ntd->dev);
	if (error != 0)
		goto fail_added;

	if (NTD_DEVT(i))
		device_create(&ntd_class,
			ntd->dev.parent,
			NTD_DEVT(i) + 1,
			NULL,
			"ntd%dro",
			i);

	mutex_unlock(&ntd_table_mutex);
	/* We _know_ we aren't being removed, because
	   our caller is still holding us here. So none
	   of this try_ nonsense, and no bitching about it
	   either. :)
	*/
	/*
	__module_get(THIS_MODULE);
	*/
	return 0;

fail_added:
	idr_remove(&ntd_idr, i);
fail_locked:
	mutex_unlock(&ntd_table_mutex);
	return 1;
}

/*****************************************************************************
*Name         :del_ntd_device
*Description  :unregister an NTD device
*Parameter    : *	@ntd: pointer to NTD device info structure
*Return       :
*Note         :
 *	Remove a device from the list of NTD devices present in the system,
 *	and notify each currently active NTD 'user' of its departure.
 *	Returns zero on success or 1 on failure, which currently will happen
 *	if the requested device does not appear to be present in the list.
*****************************************************************************/
int del_ntd_device(struct ntd_info *ntd)
{
	int ret;

	mutex_lock(&ntd_table_mutex);

	if (idr_find(&ntd_idr, ntd->index) != ntd) {
		ret = -ENODEV;
		goto out_error;
	}

	if (ntd->usecount) {
		aml_nand_msg("Removing NTD device#%ld (%s) with use count %d\n",
			ntd->index,
			ntd->name,
			ntd->usecount);
		ret = -EBUSY;
	} else {
		device_unregister(&ntd->dev);
		idr_remove(&ntd_idr, ntd->index);
		/* module_put(THIS_MODULE); */
		ret = 0;
	}

out_error:
	mutex_unlock(&ntd_table_mutex);
	return ret;
}

/*****************************************************************************
*Name         :ntd_device_register
*Description  :register an NTD device.
*Parameter    :
 * @master: the NTD device to register
 * @parts: the partitions to register - only valid if nr_parts > 0
 * @nr_parts: the number of partitions in parts.  If zero then the full NTD
 *            device is registered
*Return       :
*Note         :
 * Register an NTD device with the system and optionally, a number of
 * partitions.  If nr_parts is 0 then the whole device is registered, otherwise
 * only the partitions are registered.  To register both the full device *and*
 * the partitions, call ntd_device_register() twice, once with nr_parts == 0
 * and once equal to the number of partitions.
*****************************************************************************/
/* int ntd_device_register(struct ntd_info *master,
	const struct ntd_partition *parts,
	int nr_parts) */
/* { */
/* return parts ? add_ntd_partitions(master) : add_ntd_device(master); */
/* } */
/* EXPORT_SYMBOL_GPL(ntd_device_register); */

/*****************************************************************************
*Name         :ntd_device_unregister
*Description  :unregister an existing NTD device.
*Parameter    :
 * @master: the NTD device to unregister.  This will unregister both the master
 *          and any partitions if registered.
*Return       :
*Note         :
*****************************************************************************/
/* int ntd_device_unregister(struct ntd_info *master) */
/* { */
/* int err; */
/*  */
/* err = del_ntd_partitions(master); */
/* if (err) */
/* return err; */
/*  */
/* if (!device_is_registered(&master->dev)) */
/* return 0; */
/*  */
/* return del_ntd_device(master); */
/* } */
/* EXPORT_SYMBOL_GPL(ntd_device_unregister); */

/*****************************************************************************
*Name         : get_ntd_device
*Description  : obtain a validated handle for an NTD device
*Parameter    :
 *	@ntd: last known address of the required NTD device
 *	@num: internal device number of the required NTD device
*Return       :
*Note         :
 *	Given a number and NULL address, return the num'th entry in the device
 *	table, if any.	Given an address and num == -1, search the device table
 *	for a device with that address and return if it's still present. Given
 *	both, return the num'th driver only if its address matches. Return
 *	error code if not.
*****************************************************************************/
struct ntd_info *get_ntd_device(struct ntd_info *ntd, int num)
{
	struct ntd_info *ret = NULL, *other = NULL;
	int err = -ENODEV;

	mutex_lock(&ntd_table_mutex);

	if (num == -1) {
		ntd_for_each_device(other) {
			if (other == ntd) {
				ret = ntd;
				break;
			}
		}
	} else if (num >= 0) {
		ret = idr_find(&ntd_idr, num);
		if (ntd && ntd != ret)
			ret = NULL;
	}

	if (!ret) {
		ret = ERR_PTR(err);
		goto out;
	}

	err = __get_ntd_device(ret);
	if (err)
		ret = ERR_PTR(err);
out:
	mutex_unlock(&ntd_table_mutex);
	return ret;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int __get_ntd_device(struct ntd_info *ntd)
{
	int err;

	if (!try_module_get(ntd->owner))
		return -ENODEV;

	if (ntd->get_device) {
		err = ntd->get_device(ntd);

	/* if (err) { */
	/* module_put(ntd->owner); */
	/* return err; */
	/* } */
	}

	ntd->usecount++;
	return 0;
}

/*****************************************************************************
*Name         :
*Description  :
    *	get_ntd_device_nm - obtain a validated handle for an NTD device by
    *	device name
*Parameter    :  @name: NTD device name to open
*Return       :
*Note         :
 *	This function returns NTD device description structure in case of
 *	success and an error code in case of failure.
*****************************************************************************/
struct ntd_info *get_ntd_device_nm(const char *name)
{
	int err = -ENODEV;
	struct ntd_info *ntd = NULL, *other = NULL;

	mutex_lock(&ntd_table_mutex);

	ntd_for_each_device(other) {
		if (!strcmp(name, other->name)) {
			ntd = other;
			break;
		}
	}

	if (!ntd)
		goto out_unlock;

	err = __get_ntd_device(ntd);
	if (err)
		goto out_unlock;

	mutex_unlock(&ntd_table_mutex);
	return ntd;

out_unlock:
	mutex_unlock(&ntd_table_mutex);
	return ERR_PTR(err);
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
void put_ntd_device(struct ntd_info *ntd)
{
	mutex_lock(&ntd_table_mutex);
	__put_ntd_device(ntd);
	mutex_unlock(&ntd_table_mutex);

}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
void __put_ntd_device(struct ntd_info *ntd)
{
	--ntd->usecount;
	BUG_ON(ntd->usecount < 0);

	if (ntd->put_device)
		ntd->put_device(ntd);

	module_put(ntd->owner);
}

/*****************************************************************************
*Name         : ntd_kmalloc_up_to
*Description  :allocate a contiguous buffer up to the specified size
*Parameter    :
 * @size: A pointer to the ideal or maximum size of the allocation. Points
 *        to the actual allocation size on success
*Return       :
*Note         :
 * This routine attempts to allocate a contiguous kernel buffer up to
 * the specified size, backing off the size of the request exponentially
 * until the request succeeds or until the allocation size falls below
 * the system page size. This attempts to make sure it does not adversely
 * impact system performance, so when allocating more than one page, we
 * ask the memory allocator to avoid re-trying, swapping, writing back
 * or performing I/O.
 *
 * Note, this function also makes sure that the allocated buffer is aligned to
 * the NTD device's min. I/O unit, i.e. the "ntd->writesize" value.
 *
 * This is called, for example by ntd_{read,write} and jffs2_scan_medium,
 * to handle smaller (i.e. degraded) buffer allocations under low- or
 * fragmented-memory situations where such reduced allocations, from a
 * requested ideal, are allowed.
 *
 * Returns a pointer to the allocated buffer on success; otherwise, NULL.
*****************************************************************************/
void *ntd_kmalloc_up_to(const struct ntd_info *ntd, size_t *size)
{
	gfp_t flags;
	size_t min_alloc = max_t(size_t, ntd->pagesize, PAGE_SIZE);
	void *kbuf;

	flags = __GFP_NOWARN | __GFP_WAIT | __GFP_NORETRY | __GFP_NO_KSWAPD;
	*size = min_t(size_t, *size, KMALLOC_MAX_SIZE);

	while (*size > min_alloc) {
		kbuf = kmalloc(*size, flags);
		if (kbuf)
			return kbuf;

		*size >>= 1;
		*size = ALIGN(*size, ntd->pagesize);
	}

	/*
	 * For the last resort allocation allow 'kmalloc()' to do all sorts of
	 * things (write-back, dropping caches, etc) by using GFP_KERNEL.
	 */
	return kmalloc(*size, GFP_KERNEL);
}

/* EXPORT_SYMBOL_GPL(get_ntd_device); */
/* EXPORT_SYMBOL_GPL(get_ntd_device_nm); */
/* EXPORT_SYMBOL_GPL(__get_ntd_device); */
/* EXPORT_SYMBOL_GPL(put_ntd_device); */
/* EXPORT_SYMBOL_GPL(__put_ntd_device); */
EXPORT_SYMBOL_GPL(ntd_kmalloc_up_to);

#ifdef CONFIG_PROC_FS

/*====================================================================*/
/* Support for /proc/ntd */

static struct proc_dir_entry *proc_ntd;

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static int ntd_proc_show(struct seq_file *m, void *v)
{
	struct ntd_info *ntd;

	seq_puts(m, "dev:    block_num   blocksize  name\n");
	mutex_lock(&ntd_table_mutex);
	ntd_for_each_device(ntd) {
		seq_printf(m, "ntd%ld: %8.8llx %8.8lx \"%s\"\n",
			ntd->index,
			(unsigned long long)ntd->size,
			ntd->blocksize,
			ntd->name);
	}
	mutex_unlock(&ntd_table_mutex);
	return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static int ntd_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, ntd_proc_show, NULL);
}

static const struct file_operations ntd_proc_ops = {
	.open = ntd_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif /* CONFIG_PROC_FS */

/*====================================================================*/
/* Init code */

/* static int __init ntd_bdi_init(struct backing_dev_info *bdi,
	const char *name) */
/* { */
/* int ret; */
/*  */
/* ret = bdi_init(bdi); */
/* if (!ret) */
/* ret = bdi_register(bdi, NULL, name); */
/*  */
/* if (ret) */
/* bdi_destroy(bdi); */
/*  */
/* return ret; */
/* } */

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
/* static int __init init_ntd(void) */
static int init_ntd(void)
{
	int ret;

	ret = class_register(&ntd_class);
	if (ret)
		goto err_reg;

	/*
	ret = ntd_bdi_init(&ntd_bdi_unmappable, "ntd-unmap");
	if (ret)
		goto err_bdi1;
	*/

#ifdef CONFIG_PROC_FS
	proc_ntd = proc_create("ntd", 0, NULL, &ntd_proc_ops);
#endif /* CONFIG_PROC_FS */

	register_chrdev_region(MKDEV(NTD_CHAR_MAJOR, 0), 20, "ntd");

	return 0;

	/* err_bdi1: */
	/* class_unregister(&ntd_class); */
err_reg:
	pr_err("Error registering ntd class or bdi: %d\n", ret);
	return ret;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
/* static void __exit cleanup_ntd(void) */
/*
static void cleanup_ntd(void)
{
#ifdef CONFIG_PROC_FS
	if (proc_ntd)
		remove_proc_entry( "ntd", NULL);
#endif
	class_unregister(&ntd_class);
//	bdi_destroy(&ntd_bdi_unmappable);
}
*/
/* module_init(init_ntd); */
/* module_exit(cleanup_ntd); */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Woodhouse <dwmw2@infradead.org>");
/* MODULE_DESCRIPTION("Core NTD registration and access routines"); */
