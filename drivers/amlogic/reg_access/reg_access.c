// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/kasan.h>
#include <linux/highmem.h>

struct aml_ddev {
	unsigned long cached_reg_addr;
	unsigned long size;
	int (*debugfs_reg_access)(struct aml_ddev *indio_dev,
				  unsigned long reg, unsigned int writeval,
				  unsigned int *readval);
};

static struct aml_ddev paddr_dev;
static struct aml_ddev pdump_dev;
static struct aml_ddev vaddr_dev;
static struct aml_ddev vdump_dev;

int aml_reg_access(struct aml_ddev *indio_dev,
		   unsigned long reg, unsigned int writeval,
		   unsigned int *readval)
{
	void __iomem *vaddr;

	reg = round_down(reg, 0x3);
	vaddr = ioremap(reg, 0x4);
	if (!vaddr)
		return -ENOMEM;

	if (readval)
		*readval = readl_relaxed(vaddr);
	else
		writel_relaxed(writeval, vaddr);
	iounmap(vaddr);
	return 0;
}

int aml_mem_access(struct aml_ddev *indio_dev,
		   unsigned long addr, unsigned int writeval,
		   unsigned int *readval)
{
	void *vaddr;
	struct page *page = pfn_to_page(addr >> PAGE_SHIFT);

	vaddr = kmap(page) + (addr & ~PAGE_MASK);
	kasan_disable_current();
	if (readval)
		*readval = *(unsigned int *)vaddr;
	else
		*(unsigned int *)vaddr = writeval;
	kasan_enable_current();
	kunmap(page);
	return 0;
}

int aml_vaddr_access(struct aml_ddev *indio_dev,
		   unsigned long vaddr, unsigned int writeval,
		   unsigned int *readval)
{
	int ret;

	if (readval)
		ret = probe_kernel_read(readval, (void *)vaddr, sizeof(int));
	else
		ret = probe_kernel_write((void *)vaddr, (void *)&writeval, sizeof(int));
	return ret;
}

static ssize_t paddr_read_file(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	struct aml_ddev *indio_dev = file->private_data;
	char buf[80];
	unsigned int val = 0;
	ssize_t len;
	int ret = -1;

	if (indio_dev->debugfs_reg_access)
		ret = indio_dev->debugfs_reg_access(indio_dev,
					   indio_dev->cached_reg_addr,
					   0, &val);
	if (ret)
		pr_err("%s: read failed\n", __func__);

	len = snprintf(buf, sizeof(buf), "[0x%lx] = 0x%X\n",
		       indio_dev->cached_reg_addr, val);

	return simple_read_from_buffer(userbuf, count, ppos, buf, len);
}

static ssize_t paddr_write_file(struct file *file, const char __user *userbuf,
				size_t count, loff_t *ppos)
{
	struct aml_ddev *indio_dev = file->private_data;
	unsigned long reg;
	unsigned int val;
	char buf[80];
	int ret;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%lx %x", &reg, &val);

	if (pfn_valid(reg >> PAGE_SHIFT))
		indio_dev->debugfs_reg_access = aml_mem_access;
	else
		indio_dev->debugfs_reg_access = aml_reg_access;

	switch (ret) {
	case 1:
		indio_dev->cached_reg_addr = reg;
		break;
	case 2:
		indio_dev->cached_reg_addr = reg;
		ret = indio_dev->debugfs_reg_access(indio_dev, reg,
							  val, NULL);
		if (ret) {
			pr_err("%s: write failed\n", __func__);
			return ret;
		}
		break;
	default:
		return -EINVAL;
	}

	return count;
}

static const struct file_operations paddr_file_ops = {
	.open		= simple_open,
	.read		= paddr_read_file,
	.write		= paddr_write_file,
};

static ssize_t pdump_write_file(struct file *file, const char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct aml_ddev *indio_dev = s->private;
	unsigned long reg;
	unsigned int val;
	char buf[80];
	int ret;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%lx %i", &reg, &val);
	if (pfn_valid(reg >> PAGE_SHIFT))
		indio_dev->debugfs_reg_access = aml_mem_access;
	else
		indio_dev->debugfs_reg_access = aml_reg_access;
	switch (ret) {
	case 2:
		indio_dev->cached_reg_addr = reg;
		indio_dev->size = val;
		break;
	default:
		return -EINVAL;
	}

	return count;
}

static int dump_show(struct seq_file *s, void *what)
{
	unsigned int i = 0, val;
	int ret;
	struct aml_ddev *indio_dev = s->private;

	for (i = 0; i < indio_dev->size; i++) {
		ret = indio_dev->debugfs_reg_access(indio_dev,
				  indio_dev->cached_reg_addr,
				  0, &val);
		if (ret)
			pr_err("%s: read failed\n", __func__);

		seq_printf(s, "[0x%lx] = 0x%X\n",
			   indio_dev->cached_reg_addr, val);
		indio_dev->cached_reg_addr += sizeof(int);
	}
	indio_dev->cached_reg_addr -= sizeof(int) * indio_dev->size;

	return 0;
}

static int dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, dump_show, inode->i_private);
}

static const struct file_operations pdump_file_ops = {
	.open		= dump_open,
	.read		= seq_read,
	.write		= pdump_write_file,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static ssize_t vaddr_read_file(struct file *file, char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	struct aml_ddev *indio_dev = file->private_data;
	char buf[80];
	unsigned int val = 0;
	ssize_t len;
	int ret = -1;

	if (indio_dev->debugfs_reg_access)
		ret = indio_dev->debugfs_reg_access(indio_dev,
					   indio_dev->cached_reg_addr,
					   0, &val);
	if (ret)
		pr_err("%s: read failed\n", __func__);

	len = snprintf(buf, sizeof(buf), "[0x%lx] = 0x%X\n",
		       indio_dev->cached_reg_addr, val);

	return simple_read_from_buffer(userbuf, count, ppos, buf, len);
}

static ssize_t vaddr_write_file(struct file *file, const char __user *userbuf,
				size_t count, loff_t *ppos)
{
	struct aml_ddev *indio_dev = file->private_data;
	unsigned long reg;
	unsigned int val;
	char buf[80];
	int ret;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%lx %x", &reg, &val);

	indio_dev->debugfs_reg_access = aml_vaddr_access;

	switch (ret) {
	case 1:
		indio_dev->cached_reg_addr = reg;
		break;
	case 2:
		indio_dev->cached_reg_addr = reg;
		ret = indio_dev->debugfs_reg_access(indio_dev, reg,
					val, NULL);
		if (ret) {
			pr_err("%s: write failed\n", __func__);
			return ret;
		}
		break;
	default:
		return -EINVAL;
	}

	return count;
}

static const struct file_operations vaddr_file_ops = {
	.open		= simple_open,
	.read		= vaddr_read_file,
	.write		= vaddr_write_file,
};

static ssize_t vdump_write_file(struct file *file, const char __user *userbuf,
			       size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct aml_ddev *indio_dev = s->private;
	unsigned long reg;
	unsigned int val;
	char buf[80];
	int ret;

	count = min_t(size_t, count, (sizeof(buf) - 1));
	if (copy_from_user(buf, userbuf, count))
		return -EFAULT;

	buf[count] = 0;

	ret = sscanf(buf, "%lx %i", &reg, &val);

	indio_dev->debugfs_reg_access = aml_vaddr_access;
	switch (ret) {
	case 2:
		indio_dev->cached_reg_addr = reg;
		indio_dev->size = val;
		break;
	default:
		return -EINVAL;
	}

	return count;
}

static const struct file_operations vdump_file_ops = {
	.open		= dump_open,
	.read		= seq_read,
	.write		= vdump_write_file,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init aml_debug_init(void)
{
	static struct dentry *dir_aml_reg;

	if (IS_ENABLED(CONFIG_DEBUG_FS)) {
		dir_aml_reg = debugfs_create_dir("aml_reg", NULL);
		if (IS_ERR_OR_NULL(dir_aml_reg)) {
			pr_warn("failed to create debugfs directory\n");
			dir_aml_reg = NULL;
			return -ENOMEM;
		}
		debugfs_create_file("paddr", S_IFREG | 0440,
				    dir_aml_reg, &paddr_dev, &paddr_file_ops);
		debugfs_create_file("pdump", S_IFREG | 0440,
				    dir_aml_reg, &pdump_dev, &pdump_file_ops);
		debugfs_create_file("vaddr", S_IFREG | 0440,
				    dir_aml_reg, &vaddr_dev, &vaddr_file_ops);
		debugfs_create_file("vdump", S_IFREG | 0440,
				    dir_aml_reg, &vdump_dev, &vdump_file_ops);
	}

	return 0;
}

static void __exit aml_debug_exit(void)
{
}

module_init(aml_debug_init);
module_exit(aml_debug_exit);

MODULE_DESCRIPTION("Amlogic debug module");
MODULE_LICENSE("GPL");
