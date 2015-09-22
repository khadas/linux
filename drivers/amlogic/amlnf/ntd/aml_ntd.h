/*
 * drivers/amlogic/amlnf/ntd/aml_ntd.h
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


#ifndef __NTD_NTD_H__
#define __NTD_NTD_H__

#include <linux/types.h>
#include <linux/module.h>
#include <linux/uio.h>
#include <linux/notifier.h>
#include <linux/device.h>
#include <asm/div64.h>

#include "../include/amlnf_dev.h"

/*
#include "../block/aml_nftl_block.h"
*/

#define NTD_WRITEABLE		0x400	/* Device is writeable */
#define NTD_BIT_WRITEABLE	0x800	/* Single bits can be flipped */
#define NTD_NO_ERASE		0x1000	/* No erase necessary */
#define NTD_POWERUP_LOCK	0x2000	/* Always locked after reset */

#define NTDPART_OFS_NXTBLK	(-2)
#define NTDPART_OFS_APPEND	(-1)
#define NTDPART_SIZ_FULL	(0)

#define MAX_NAND_PART_NAME_LEN		16

#define NTD_BLOCK_MAJOR		31

#define NTD_FAIL_ADDR_UNKNOWN		-1LL

struct ntd_blktrans_ops;
struct ntd_blktrans_dev;

struct ntd_info {
	uint64_t offset;
	uint64_t size;	 /* Total size of the NTD */
	unsigned long flags;
	unsigned long blocksize;
	unsigned long pagesize;
	unsigned long oobsize;
	unsigned long blocksize_shift;
	unsigned long pagesize_shift;
	unsigned long blocksize_mask;
	unsigned long pagesize_mask;
	char *name;
	long index;

	struct ntd_partition *parts;
	unsigned long nr_partitions;

	/*
	Backing device capabilities for this device provides mmap capabilities
	*/
	/*
	struct backing_dev_info *backing_dev_info;
	*/
	int (*erase)(struct ntd_info *ntd, uint block);
	int (*read_page_with_oob)(struct ntd_info *ntd,
		uint page,
		u_char *oob_buf,
		u_char *buf);
	int (*write_page_with_oob)(struct ntd_info *ntd,
		uint page,
		u_char *oob_buf,
		u_char *buf);
	int (*read_only_oob)(struct ntd_info *ntd,
		u_int32_t page,
		u_char *oob_buf);
	int (*block_isbad)(struct ntd_info *ntd, u_int32_t block);
	int (*block_markbad)(struct ntd_info *ntd, u_int32_t block);

	/* int (*lock) (struct ntd_info *ntd, loff_t ofs, size_t len); */
	/* int (*unlock) (struct ntd_info *ntd, loff_t ofs, size_t len); */
	/* int (*is_locked) (struct ntd_info *ntd, loff_t ofs, uint64_t len); */
	/* void (*sync) (struct ntd_info *ntd); */

	int (*get_device)(struct ntd_info *ntd);
	int (*put_device)(struct ntd_info *ntd);
	int (*flush)(struct ntd_info *ntd);
	int (*suspend)(struct ntd_info *ntd);
	void (*resume)(struct ntd_info *ntd);

	struct notifier_block reboot_notifier;
	void *priv;
	void *nftl_priv;
	struct module *owner;
	struct device dev;
	int usecount;
	int thread_stop_flag;
	unsigned long badblocks;
	struct list_head list;

	/* If the driver is something smart, like UBI, it may need to maintain
	 * its own reference counting. The below functions are only for driver.
	 * The driver may register its callbacks. These callbacks are not
	 * supposed to be called by NTD users */
	/* int (*get_device) (struct ntd_info *ntd); */
	/* void (*put_device) (struct ntd_info *ntd); */
};


/*
struct ntd_partition {
	char *name;
	uint64_t size;
	uint64_t offset;
	uint mask_flags;
};
*/

struct ntd_partition {
	char name[MAX_NAND_PART_NAME_LEN]; /* identifier string */
	uint64_t size; /* partition size */
	uint64_t offset; /* offset within the master space */
	u32 mask_flags; /* master flags to mask out for this partition */
	void *priv;
};

struct request;
struct hd_geometry;

struct ntd_blktrans_ops {
	char *name;
	int major;
	int part_bits;
	int blksize;
	int blkshift;

	/* Access functions */
	void (*update_blktrans_sysinfo)(struct ntd_blktrans_dev *dev,
		unsigned int cmd,
		unsigned long arg);
	/* for nftl could do much requests once a time not just 1page a time */
	int (*do_blktrans_request)(struct ntd_blktrans_ops *tr,
		struct ntd_blktrans_dev *dev,
		struct request *req);

	int (*readsect)(struct ntd_blktrans_dev *dev,
		unsigned long block,
		char *buffer);
	int (*writesect)(struct ntd_blktrans_dev *dev,
		unsigned long block,
		char *buffer);
	int (*discard)(struct ntd_blktrans_dev *dev,
		unsigned long block,
		unsigned int nr_blocks);
	void (*background)(struct ntd_blktrans_dev *dev);

	/* Block layer ioctls */
	int (*getgeo)(struct ntd_blktrans_dev *dev, struct hd_geometry *geo);
	int (*flush)(struct ntd_blktrans_dev *dev);

	/* Called with ntd_table_mutex held; no race with add/remove */
	int (*open)(struct ntd_blktrans_dev *dev);
	int (*release)(struct ntd_blktrans_dev *dev);
	int (*wipe_part)(struct ntd_blktrans_dev *dev);

	/*
	 Called on {de,}registration and on subsequent addition/removal
	   of devices, with ntd_table_mutex held.
	*/
	void (*add_ntd)(struct ntd_blktrans_ops *tr, struct ntd_info *ntd);
	void (*remove_dev)(struct ntd_blktrans_dev *dev);

	struct list_head devs;
	struct list_head list;
	struct module *owner;
};

struct ntd_blktrans_dev {
	struct ntd_blktrans_ops	*tr;
	struct list_head list;
	struct ntd_info	*ntd;
	struct device dev;
	struct mutex lock;
	int devnum;
	bool bg_stop;
	unsigned long size;
	int readonly;
	int open;
	struct kref ref;
	struct gendisk *disk;
	struct attribute_group *disk_attributes;
	struct task_struct *thread;
	struct request_queue *rq;
	spinlock_t queue_lock;
	void *priv;
	char  name[24];
};

static inline struct ntd_info *dev_to_ntd(struct device *dev)
{
	return dev ? dev_get_drvdata(dev) : NULL;
}

static inline uint ntd_div_by_eb(uint64_t sz, struct ntd_info *ntd)
{
	if (ntd->blocksize_shift)
		return sz >> ntd->blocksize_shift;
	do_div(sz, ntd->blocksize);
	return sz;
}

static inline uint ntd_mod_by_eb(uint64_t sz, struct ntd_info *ntd)
{
	if (ntd->blocksize_shift)
		return sz & ntd->blocksize_mask;
	return do_div(sz, ntd->blocksize);
}

static inline uint ntd_div_by_ws(uint64_t sz, struct ntd_info *ntd)
{
	if (ntd->pagesize_shift)
		return sz >> ntd->pagesize_shift;
	do_div(sz, ntd->pagesize);
	return sz;
}

static inline uint ntd_mod_by_ws(uint64_t sz, struct ntd_info *ntd)
{
	if (ntd->pagesize_shift)
		return sz & ntd->pagesize_mask;
	return do_div(sz, ntd->pagesize);
}

	/* Kernel-side ioctl definitions */
extern int add_ntd_device(struct ntd_info *ntd);
extern int del_ntd_device(struct ntd_info *ntd);


struct ntd_partition;
struct amlnand_phydev;
extern int ntd_device_register(struct ntd_info *master,
	const struct ntd_partition *parts,
	int nr_parts);
extern int ntd_device_unregister(struct ntd_info *master);
extern struct ntd_info *get_ntd_device(struct ntd_info *ntd, int num);
extern int __get_ntd_device(struct ntd_info *ntd);
extern void __put_ntd_device(struct ntd_info *ntd);
extern struct ntd_info *get_ntd_device_nm(const char *name);
extern void put_ntd_device(struct ntd_info *ntd);

extern void blktrans_notify_remove(struct ntd_info *ntd);
extern void blktrans_notify_add(struct ntd_info *ntd);

extern struct mutex ntd_table_mutex;
/* EXPORT_SYMBOL(ntd_table_mutex); */
extern struct ntd_info *__ntd_next_device(int i);

extern int add_ntd_device(struct ntd_info *ntd);
extern int del_ntd_device(struct ntd_info *ntd);
extern int add_ntd_partitions(struct amlnand_phydev *master);
extern int del_ntd_partitions(struct amlnand_phydev *master);
extern int add_ntd_blktrans_dev(struct ntd_blktrans_dev *new);
extern int del_ntd_blktrans_dev(struct ntd_blktrans_dev *old);
extern int register_ntd_blktrans(struct ntd_blktrans_ops *tr);
extern int deregister_ntd_blktrans(struct ntd_blktrans_ops *tr);

/********aml_nrdpart.c*********/
extern int aml_ntd_nftl_flush(struct ntd_info *ntd);

#define ntd_for_each_device(mtd)			\
	for ((ntd) = __ntd_next_device(0);		\
	     (ntd) != NULL;				\
	     (ntd) = __ntd_next_device(ntd->index + 1))



/* struct ntd_notifier { */
/* void (*add)(struct ntd_info *ntd); */
/* void (*remove)(struct ntd_info *ntd); */
/* struct list_head list; */
/* }; */

/* extern void register_ntd_user (struct ntd_notifier *new); */
/* extern int unregister_ntd_user (struct ntd_notifier *old); */

void *ntd_kmalloc_up_to(const struct ntd_info *ntd, size_t *size);
extern int blk_class_register(struct class *cls);
extern int blk_device_register(struct device *dev, int num);
#endif /* __NTD_NTD_H__ */

