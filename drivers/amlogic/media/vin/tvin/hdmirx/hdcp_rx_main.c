// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/miscdevice.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>
#include <linux/netlink.h>
#include <linux/proc_fs.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#include "hdcp_rx_main.h"
#include "hdmi_rx_repeater.h"
#include "hdmi_rx_drv.h"
#include "hdmi_rx_hw.h"

#define MAX_ESM_DEVICES 6
#define ESM_RX_HPI_BASE    0xd0076000
static bool randomize_mem;
static int is_esmmem_created;

/*
 * module_param(randomize_mem, bool, 0664);
 * MODULE_PARM_DESC(noverify, "Wipe mem allocations on startup (for debug)");
 */

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

	struct debugfs_blob_wrapper blob;

	struct resource *hpi_resource;
	u8 __iomem *hpi;
};

static struct esm_device esm_devices[MAX_ESM_DEVICES];
static const char *MY_TAG = "ESM HLD: ";

/* get_meminfo - ESM_IOC_MEMINFO implementation */
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

/* load_code - ESM_IOC_LOAD_CODE implementation */
static long load_code(struct esm_device *esm,
		      struct esm_ioc_code __user *arg)
{
	struct esm_ioc_code head;

	if (copy_from_user(&head, arg, sizeof(head)) != 0)
		return -EFAULT;

	if (head.len > esm->code_size)
		return -ENOSPC;

	/* if (esm->code_loaded) */
	/*	return -EBUSY; */

	if (copy_from_user(esm->code, &arg->data, head.len) != 0)
		return -EFAULT;
	pr_info("load code...\n");

	return 0;
}

/* write_data - ESM_IOC_WRITE_DATA implementation */
static long write_data(struct esm_device *esm,
		       struct esm_ioc_data __user *arg)
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

/* read_data - ESM_IOC_READ_DATA implementation */
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

/* set_data - ESM_IOC_MEMSET_DATA implementation */
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

/* hpi_read - ESM_IOC_READ_HPI implementation */
static long hpi_read(struct esm_device *esm, void __user *arg)
{
	struct esm_ioc_hpi_reg reg;

	if (copy_from_user(&reg, arg, sizeof(reg)) != 0)
		return -EFAULT;

	if ((reg.offset & 3) || reg.offset >= resource_size(esm->hpi_resource))
		return -EINVAL;

	reg.value = rx_hdcp22_rd(reg.offset);

	if (copy_to_user(arg, &reg, sizeof(reg)) != 0)
		return -EFAULT;

	return 0;
}

/* hpi_write - ESM_IOC_WRITE_HPI implementation */
static long hpi_write(struct esm_device *esm, void __user *arg)
{
	struct esm_ioc_hpi_reg reg;

	if (copy_from_user(&reg, arg, sizeof(reg)) != 0)
		return -EFAULT;

	if ((reg.offset & 3) || reg.offset >= resource_size(esm->hpi_resource))
		return -EINVAL;

	rx_hdcp22_wr_reg(reg.offset, reg.value);
	return 0;
}

/* alloc_esm_slot - alloc_esm_slot*/
static struct esm_device *alloc_esm_slot(const struct esm_ioc_meminfo *info)
{
	int i;

	/* Check if we have a matching device (same HPI base) */
	for (i = 0; i < MAX_ESM_DEVICES; i++) {
		struct esm_device *slot = &esm_devices[i];
		/* Modified for SWPL-15872:
		 * when esm reboot,hpi_base will be the value defined
		 * in driver dts,not be hpi_resource->start,and it will
		 * not enter if(), and esm will use a new slot,
		 * and cause esm init fail.
		 */
		/* if ((slot->allocated) && (info->hpi_base) == */
		/*		(slot->hpi_resource->start)) */
		if (slot->allocated)
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

/* free_dma_areas - free_dma_areas*/
static void free_dma_areas(struct esm_device *esm)
{
	if (!esm->code_is_phys_mem && esm->code) {
		dma_free_coherent(hdmirx_dev, esm->code_size,
				  esm->code, esm->code_base);
		esm->code = NULL;
	}

	if (!esm->data_is_phys_mem && esm->data) {
		dma_free_coherent(hdmirx_dev, esm->data_size,
				  esm->data, esm->data_base);
		esm->data = NULL;
	}
}

static struct dentry *esm_rx_debugfs;
struct dentry *esm_rx_blob;

static int alloc_dma_areas(struct esm_device *esm,
			   const struct esm_ioc_meminfo *info)
{
	esm->code_size = info->code_size;
	esm->code_is_phys_mem = (info->code_base != 0);

	if (esm->code_is_phys_mem) {
		/* TODO: support highmem */
		esm->code_base = info->code_base;
		esm->code = phys_to_virt(esm->code_base);
	} else {
		esm->code = dma_alloc_coherent(hdmirx_dev, esm->code_size,
					       &esm->code_base, GFP_KERNEL);
		if (!esm->code)
			return -ENOMEM;
	}

	esm->data_size = info->data_size;
	esm->data_is_phys_mem = (info->data_base != 0);

	if (esm->data_is_phys_mem) {
		esm->data_base = info->data_base;
		esm->data = phys_to_virt(esm->data_base);
	} else {
		esm->data = dma_alloc_coherent(hdmirx_dev, esm->data_size,
					       &esm->data_base, GFP_KERNEL);
		if (!esm->data) {
			free_dma_areas(esm);
			return -ENOMEM;
		}
	}

	esm_rx_debugfs = debugfs_create_dir("esm_rx", NULL);
	if (!esm_rx_debugfs)
		return -ENOENT;

	esm->blob.data = (void *)esm->data;
	esm->blob.size = esm->data_size;
	esm_rx_blob = debugfs_create_blob("blob",
					  0644,
					  esm_rx_debugfs,
					  &esm->blob);

	if (randomize_mem) {
		prandom_bytes(esm->code, esm->code_size);
		prandom_bytes(esm->data, esm->data_size);
	}
	if (!is_esmmem_created) {
		if (esm->data_base && esm->code_base) {
			is_esmmem_created = 1;
			pr_info("create /dev/esmmem\n");
		}
	}
	return 0;
}

/* init - ESM_IOC_INIT implementation */
static long init(struct file *f, void __user *arg)
{
	struct resource *hpi_mem;
	struct esm_ioc_meminfo info;
	struct esm_device *esm;
	int rc;

	pr_info("HDCP_RX22 VERSION: %s\n", HDCP_RX22_VER);
	pr_info("ESM VERSION: %s\n", ESM_VERSION);

	if (copy_from_user(&info, arg, sizeof(info)) != 0)
		return -EFAULT;

	esm = alloc_esm_slot(&info);
	if (!esm)
		return -EMFILE;

	if (!esm->initialized) {
		rc = alloc_dma_areas(esm, &info);
		if (rc < 0)
			goto err_free;
		info.hpi_base =
			rx_reg_maps[MAP_ADDR_MODULE_HDMIRX_CAPB3].phy_addr;
		hpi_mem = request_mem_region(info.hpi_base, 128, "esm-hpi");
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

/*
 * set_param - set kernel param to hdcp_rx22
 * amlogic added
 */
static long set_param(struct esm_device *esm,
		      struct esm_ioc_param __user *arg)
{
	struct esm_ioc_param head;

	memset(&head, 0, sizeof(head));
	if (copy_from_user(&head, arg, sizeof(head)) != 0)
		return -EFAULT;
	esm_auth_fail_en = head.reauth;
	esm_reset_flag = head.esm_reset;
	hdcp22_stop_auth = head.auth_stop;
	hdcp22_esm_reset2 = head.esm_reset2;
	return 0;
}

/*
 * get_param - hdcp_rx22 get param from kernel
 * amlogic added
 */
static long get_param(struct esm_device *esm,
		      struct esm_ioc_param __user *arg)
{
	struct esm_ioc_param head;

	head.hpd = hpd_to_esm;
	head.video_stable = video_stable_to_esm;
	head.pwr_sts = pwr_sts_to_esm;
	head.log = enable_hdcp22_esm_log;
	head.esm_reset = esm_reset_flag;
	head.reauth = esm_auth_fail_en;
	head.esm_err = 0;
	head.auth_stop = hdcp22_stop_auth;
	head.esm_reset2 = hdcp22_esm_reset2;
	head.esm_kill = hdcp22_kill_esm;
	head.reset_mode = esm_recovery_mode;

	if (copy_to_user(arg, &head, sizeof(head)) != 0)
		return -EFAULT;

	return 0;
}

/* free_esm_slot - free_esm_slot*/
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

/* hld_ioctl - hld_ioctl*/
static long hld_ioctl(struct file *f,
		      unsigned int cmd,
		      unsigned long arg)
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
	case ESM_IOC_GET_PARAM:
		return get_param(esm, data);
	case ESM_IOC_SET_PARAM:
		return set_param(esm, data);
	}

	return -ENOTTY;
}

static const struct file_operations hld_file_operations = {
	.unlocked_ioctl = hld_ioctl,
	#ifdef CONFIG_COMPAT
	.compat_ioctl = hld_ioctl,
	#endif
	.owner = THIS_MODULE,
};

static struct miscdevice hld_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "esm_rx",
	.fops = &hld_file_operations,
};

int __init hld_init(void)
{
	pr_info("%sInitializing...\n", MY_TAG);
	return misc_register(&hld_device);
}

void __exit hld_exit(void)
{
	int i;

	misc_deregister(&hld_device);

	for (i = 0; i < MAX_ESM_DEVICES; i++)
		free_esm_slot(&esm_devices[i]);
}

/*
 * readSys-read sysfs
 */
void readSys(const char *path, char *buf, int count)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	mm_segment_t old_fs = get_fs();

	if (!buf) {
		rx_pr("buf is NULL");
		return;
	}
	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDONLY, 0);

	if (IS_ERR(filp)) {
		rx_pr("create %s error.\n", path);
		return;
	}

	vfs_read(filp, buf, count, &pos);

	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);
}

/*
 * writeSys-write buffer into sysfs
 */

int writeSys(const char *path, const char *val)
{
	struct file *filp = NULL;
	loff_t pos = 0;
	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDWR, 0);

	if (IS_ERR(filp)) {
		rx_pr("create %s error.\n", path);
		return -1;
	}

	vfs_write(filp, val, strlen(val), &pos);

	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	set_fs(old_fs);
	return 0;
}

/*
 * is_rx_unifykey_exist-check rx hdcp key exist
 * for debug
 */
int is_rx_unifykey_exist(const char *key_type)
{
	char existValue[10] = {0};
	/*char secValue[10] = {0};*/
	int ret = -1;

	if (writeSys(HDCP_RX_KEY_ATTACH_DEV_PATH, "1")) {
		rx_pr("set %s attach failed!\n", key_type);
		return ret;
	}

	if (writeSys(HDCP_RX_KEY_NAME_DEV_PATH, key_type)) {
		rx_pr("set %s name failed!\n", key_type);
		return ret;
	}

	readSys(HDCP_RX_KEY_DATA_EXIST, (char *)existValue, 10);
	if (!strncmp(existValue, "exist", 5)) {
		rx_pr("%s exist\n", key_type);
		ret = 1;
	} else {
		rx_pr("%s not exist\n", key_type);
		ret = 0;
	}

	//for further debug using
	/*readSys(HDCP_RX_KEY_SECURE_DEV_PATH, (char*)secValue, 10);*/
	/*if (!strncmp(secValue, "true", 4))*/
		/*rx_pr("hdcp14_rx secure key\n");*/
	/*else*/
		/*rx_pr("hdcp14_rx normal key\n");*/
	return ret;
}

int rx_unifykey_query(int index)
{
	return rx_smc_cmd_handler(index, 0);
}

int is_rx_unifykey_14_support(void)
{
	return rx_unifykey_query(HDCP14_RX_QUERY);
}

int is_rx_unifykey_22_support(void)
{
	return (rx_unifykey_query(HDCP22_RX_FW_QUERY) &
		rx_unifykey_query(HDCP22_RX_PRIVATE_QUERY));
}

int is_rx_unifykey_support(void)
{
	return (is_rx_unifykey_14_support() | is_rx_unifykey_22_support());
}

int is_rx_unifykey_rprx_support(void)
{
	return 0;/*return rx_unifykey_query(HDCP22_RPRX_FW_QUERY);*/
}

int is_rx_unifykey_rprp_support(void)
{
	return 0;/*return rx_unifykey_query(HDCP22_RPRP_FW_QUERY);*/
}

int is_rx_unifykey_rp_private_support(void)
{
	return 0;/*return rx_unifykey_query(HDCP22_RP_PRIVATE_QUERY);*/
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Synopsys, Inc.");
MODULE_DESCRIPTION("ESM Linux Host Library Driver");
