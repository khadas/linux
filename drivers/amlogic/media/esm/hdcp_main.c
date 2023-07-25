// ------------------------------------------------------------------------
//
//              (C) COPYRIGHT 2014 - 2015 SYNOPSYS, INC.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2
// as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// ------------------------------------------------------------------------
//
// Project:
//
// ESM Host Library
//
// Description:
//
// ESM Host Library Driver: Linux kernel module
//
// ------------------------------------------------------------------------

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/miscdevice.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/of_device.h>
#include <linux/debugfs.h>
#include <linux/slab.h>

#include "hdcp.h"

#define ELP_DEBUG()	pr_info("esm %s[%d]\n", __func__, __LINE__)

#define MAX_ESM_DEVICES 6
#define MAX_ESM_SIZE 0x50000

static int verbose;

static bool randomize_mem;
module_param(randomize_mem, bool, 0644);
MODULE_PARM_DESC(noverify, "Wipe memory allocations on startup (for debug)");
static long hld_ioctl(struct file *f, unsigned int cmd, unsigned long arg);

static const struct file_operations hld_file_operations = {
	.unlocked_ioctl = hld_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = hld_ioctl,
#endif
	.owner = THIS_MODULE,
};

static struct miscdevice hld_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "esm",
	.fops = &hld_file_operations,
};

static struct miscdevice esm_code_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "esm_code",
	.fops = &hld_file_operations,
};

static struct miscdevice esm_data_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "esm_data",
	.fops = &hld_file_operations,
};

struct esm_device {
	int allocated, initialized;
	int code_loaded;

	int code_is_phys_mem;
	dma_addr_t code_base;
	u32 code_size;
	u8 *code;

	int data_is_phys_mem;
	dma_addr_t data_base;
	u32 data_size;
	u8 *data;

	struct dentry *esm_blob;
	struct debugfs_blob_wrapper blob;
	struct resource *hpi_resource;
	u8 __iomem *hpi;
};

static struct esm_device esm_devices[MAX_ESM_DEVICES];

/* ESM_IOC_MEMINFO implementation */
static long get_meminfo(struct esm_device *esm, void __user *arg)
{
	struct esm_ioc_meminfo info = {
		.hpi_base  = esm->hpi_resource->start,
		.code_base = esm->code_base,
		.code_size = esm->code_size,
		.data_base = esm->data_base,
		.data_size = esm->data_size,
	};

	if (copy_to_user(arg, &info, sizeof(info)) != 0)
		return -EFAULT;

	return 0;
}

/* ESM_IOC_LOAD_CODE implementation */
static long load_code(struct esm_device *esm, struct esm_ioc_code __user *arg)
{
	struct esm_ioc_code head;

	if (copy_from_user(&head, arg, sizeof(head)) != 0)
		return -EFAULT;

	if (head.len > esm->code_size)
		return -ENOSPC;

	if (esm->code_loaded)
		return 0; /* return -EBUSY; */

	if (copy_from_user(esm->code, &arg->data, head.len) != 0)
		return -EFAULT;

	/* esm->code_loaded = 1; */
	return 0;
}

/* ESM_IOC_WRITE_DATA implementation */
static long write_data(struct esm_device *esm, struct esm_ioc_data __user *arg)
{
	struct esm_ioc_data head;

	if (copy_from_user(&head, arg, sizeof(head)) != 0)
		return -EFAULT;

	if (esm->data_size < head.len)
		return -ENOSPC;
	if (esm->data_size - head.len < head.offset)
		return -ENOSPC;

	if (copy_from_user(esm->data + head.offset, &arg->data, head.len) != 0)
		return -EFAULT;

	return 0;
}

/* ESM_IOC_READ_DATA implementation */
static long read_data(struct esm_device *esm, struct esm_ioc_data __user *arg)
{
	struct esm_ioc_data head;

	if (copy_from_user(&head, arg, sizeof(head)) != 0)
		return -EFAULT;

	if (esm->data_size < head.len)
		return -ENOSPC;
	if (esm->data_size - head.len < head.offset)
		return -ENOSPC;

	if (copy_to_user(&arg->data, esm->data + head.offset, head.len) != 0)
		return -EFAULT;

	return 0;
}

/* ESM_IOC_MEMSET_DATA implementation */
static long set_data(struct esm_device *esm, void __user *arg)
{
	union {
		struct esm_ioc_data data;
		unsigned char buf[sizeof(struct esm_ioc_data) + 1];
	} u;

	if (copy_from_user(&u.data, arg, sizeof(u.data)) != 0)
		return -EFAULT;

	if (esm->data_size < u.data.len)
		return -ENOSPC;
	if (esm->data_size - u.data.len < u.data.offset)
		return -ENOSPC;

	memset(esm->data + u.data.offset, u.data.data[0], u.data.len);
	return 0;
}

/* ESM_IOC_READ_HPI implementation */
static long hpi_read(struct esm_device *esm, void __user *arg)
{
	struct esm_ioc_hpi_reg reg;

	if (copy_from_user(&reg, arg, sizeof(reg)) != 0)
		return -EFAULT;

	if ((reg.offset & 3) || reg.offset >= resource_size(esm->hpi_resource))
		return -EINVAL;

	reg.value = ioread32(esm->hpi + reg.offset);
	if (verbose)
		pr_info("R reg.value = 0x%x, reg.offset = 0x%x\n",
			reg.value, reg.offset);
	if (copy_to_user(arg, &reg, sizeof(reg)) != 0)
		return -EFAULT;

	return 0;
}

/* ESM_IOC_WRITE_HPI implementation */
static long hpi_write(struct esm_device *esm, void __user *arg)
{
	struct esm_ioc_hpi_reg reg;

	if (copy_from_user(&reg, arg, sizeof(reg)) != 0)
		return -EFAULT;

	if ((reg.offset & 3) || reg.offset >= resource_size(esm->hpi_resource))
		return -EINVAL;
	if (verbose)
		pr_info("W reg.value = 0x%x, reg.offset = 0x%x\n",
			reg.value, reg.offset);
	iowrite32(reg.value, esm->hpi + reg.offset);
	return 0;
}

static struct esm_device *alloc_esm_slot(const struct esm_ioc_meminfo *info)
{
	int i;

	/* Check if we have a matching device (same HPI base) */
	for (i = 0; i < MAX_ESM_DEVICES; i++) {
		struct esm_device *slot = &esm_devices[i];

		if (slot->allocated &&
		    info->hpi_base == slot->hpi_resource->start)
			return slot;
	}

	/* Find unused slot */
	for (i = 0; i < MAX_ESM_DEVICES; i++) {
		struct esm_device *slot = &esm_devices[i];

		if (!slot->allocated) {
			slot->allocated = 1;
			return slot;
		}
	}

	return NULL;
}

static struct dentry *esm_debugfs;
/*static struct dentry *esm_blob;*/

static void free_dma_areas(struct esm_device *esm)
{
	if (!esm->code_is_phys_mem && esm->code) {
		dma_free_coherent(NULL, esm->code_size, esm->code,
				esm->code_base);
		esm->code = NULL;
	}

	if (!esm->data_is_phys_mem && esm->data) {
		dma_free_coherent(NULL, esm->data_size, esm->data,
				esm->data_base);
		esm->data = NULL;
	}

	debugfs_remove_recursive(esm_debugfs);
}

static int alloc_dma_areas(struct esm_device *esm,
				const struct esm_ioc_meminfo *info)
{
	char blobname[32];

	esm->code_size = info->code_size;
	esm->code_is_phys_mem = (info->code_base != 0);

	misc_register(&esm_code_device);
	misc_register(&esm_data_device);
	if (esm->code_is_phys_mem) {
		/* TODO: support highmem */
		esm->code_base = info->code_base;
		esm->code = phys_to_virt(esm->code_base);
	} else {
		esm_code_device.this_device->coherent_dma_mask = DMA_BIT_MASK(32);
		esm_code_device.this_device->dma_mask =
			&esm_code_device.this_device->coherent_dma_mask;
		esm->code = dma_alloc_coherent(esm_code_device.this_device,
					esm->code_size,
					&esm->code_base,
					GFP_KERNEL);
		pr_info("the esm code address is %px\n", esm->code);
		if (!esm->code) {
			free_dma_areas(esm);
			return -ENOMEM;
		}
	}

	esm->data_size = info->data_size;
	esm->data_is_phys_mem = (info->data_base != 0);

	if (esm->data_is_phys_mem) {
		esm->data_base = info->data_base;
		esm->data = phys_to_virt(esm->data_base);
	} else {
		esm_data_device.this_device->coherent_dma_mask = DMA_BIT_MASK(32);
		esm_data_device.this_device->dma_mask =
			&esm_data_device.this_device->coherent_dma_mask;
		esm->data = dma_alloc_coherent(esm_data_device.this_device,
					       esm->data_size,
					       &esm->data_base,
					       GFP_KERNEL);
		pr_info("the esm data address is %px\n", esm->data);
		if (!esm->data) {
			free_dma_areas(esm);
			return -ENOMEM;
		}
	}

	if (randomize_mem) {
		prandom_bytes(esm->code, esm->code_size);
		prandom_bytes(esm->data, esm->data_size);
	}

	if (!esm_debugfs) {
		esm_debugfs = debugfs_create_dir("esm", NULL);
		if (!esm_debugfs)
			return -ENOENT;
	}
	memset(blobname, 0, sizeof(blobname));
	sprintf(blobname, "blob.%x", info->hpi_base);
	esm->blob.data = (void *)esm->data;
	esm->blob.size = esm->data_size;
	esm->esm_blob = debugfs_create_blob(blobname, 0644, esm_debugfs,
					    &esm->blob);

	return 0;
}

/* ESM_IOC_INIT implementation */
static long init(struct file *f, void __user *arg)
{
	struct resource *hpi_mem;
	struct esm_ioc_meminfo info;
	struct esm_device *esm;
	char region_name[20];
	int rc;

	if (copy_from_user(&info, arg, sizeof(info)) != 0)
		return -EFAULT;

	if (info.code_size > MAX_ESM_SIZE)
		info.code_size = MAX_ESM_SIZE;
	if (info.data_size > MAX_ESM_SIZE)
		info.data_size = MAX_ESM_SIZE;

	esm = alloc_esm_slot(&info);
	if (!esm)
		return -EMFILE;

	if (!esm->initialized) {
		rc = alloc_dma_areas(esm, &info);
		if (rc < 0)
			goto err_free;
		/* pr_info("info.hpi_base = 0x%x\n", info.hpi_base); */
		/* hpi_mem =
		 *	request_mem_region(info.hpi_base, 128, "esm-hpi");
		 */
		sprintf(region_name, "ESM-%X", info.hpi_base);
		pr_info("info.hpi_base = 0x%x region_name:%s\n",
			info.hpi_base, region_name);
		hpi_mem = request_mem_region(info.hpi_base, 0x100, region_name);

		if (!hpi_mem) {
			rc = -EADDRNOTAVAIL;
			goto err_free;
		}

		esm->hpi = ioremap_nocache(hpi_mem->start,
					   resource_size(hpi_mem));
		if (!esm->hpi) {
			rc = -ENOMEM;
			goto err_release_region;
		}
		esm->hpi_resource = hpi_mem;
		esm->initialized = 1;
	}

	/*every time clear the data buff*/
	if (esm->data)
		memset(esm->data, 0, esm->data_size);
	pr_info("esm data = %p size:%d\n",
		esm->data, esm->data_size);

	f->private_data = esm;
		return 0;

err_release_region:
	release_resource(hpi_mem);
err_free:
	free_dma_areas(esm);
	esm->initialized = 0;
	esm->allocated = 0;

	return rc;
}

static void free_esm_slot(struct esm_device *slot)
{
	if (!slot->allocated)
		return;

	if (slot->initialized) {
		iounmap(slot->hpi);
		release_resource(slot->hpi_resource);
		free_dma_areas(slot);
	}

	slot->initialized = 0;
	slot->allocated = 0;
}

static long hld_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct esm_device *esm = f->private_data;
	void __user *data = (void __user *)arg;

	if (cmd == ESM_IOC_INIT)
		return init(f, data);
	else if (!esm)
		return -EAGAIN;

	switch (cmd) {
	case ESM_IOC_INIT:
		return init(f, data);
	case ESM_IOC_MEMINFO:
		return get_meminfo(esm, data);
	case ESM_IOC_READ_HPI:
		return hpi_read(esm, data);
	case ESM_IOC_WRITE_HPI:
		return hpi_write(esm, data);
	case ESM_IOC_LOAD_CODE:
		return load_code(esm, data);
	case ESM_IOC_WRITE_DATA:
		return write_data(esm, data);
	case ESM_IOC_READ_DATA:
		return read_data(esm, data);
	case ESM_IOC_MEMSET_DATA:
		return set_data(esm, data);
	default:
		break;
	}

	return -ENOTTY;
}

int __init esm_init(void)
{
	return misc_register(&hld_device);
}

void __exit esm_exit(void)
{
	int i;

	misc_deregister(&hld_device);

	for (i = 0; i < MAX_ESM_DEVICES; i++)
		free_esm_slot(&esm_devices[i]);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Synopsys, Inc.");
MODULE_DESCRIPTION("ESM Linux Host Library Driver");

module_param(verbose, int, 0644);
MODULE_PARM_DESC(verbose, "Enable (1) or disable (0) the debug traces.");
