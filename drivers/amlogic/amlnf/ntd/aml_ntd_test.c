/*
 * drivers/amlogic/amlnf/ntd/aml_ntd_test.c
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


#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/mtd/mtd.h>
#include <linux/blkdev.h>
#include <linux/blkpg.h>
#include <linux/spinlock.h>
#include <linux/hdreg.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/module.h>

#include "aml_ntd.h"



/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/


/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/


/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/


/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/



#define SIMP_BLKDEV_DEVICEMAJOR	COMPAQ_SMART2_MAJOR
#define SIMP_BLKDEV_DISKNAME	"simp_blkdev"
#define SIMP_BLKDEV_BYTES	(1*1024*1024)

unsigned char simp_blkdev_data[SIMP_BLKDEV_BYTES];

static struct request_queue *simp_blkdev_queue;
static struct gendisk *simp_blkdev_disk;


static void simp_blkdev_do_request(struct request_queue *q);

struct const block_device_operations simp_blkdev_fops = {
	.owner = THIS_MODULE,
};

int aml_test(void)
{
	int ret;

	simp_blkdev_queue = blk_init_queue(simp_blkdev_do_request, NULL);
	if (!simp_blkdev_queue) {
		ret = -ENOMEM;
		goto err_init_queue;
	}

	simp_blkdev_disk = alloc_disk(1);
	if (!simp_blkdev_disk) {
		ret = -ENOMEM;
		goto err_alloc_disk;
	}

	strcpy(simp_blkdev_disk->disk_name, SIMP_BLKDEV_DISKNAME);
	simp_blkdev_disk->major = SIMP_BLKDEV_DEVICEMAJOR;
	simp_blkdev_disk->first_minor = 0;
	simp_blkdev_disk->fops = &simp_blkdev_fops;
	simp_blkdev_disk->queue = simp_blkdev_queue;
	set_capacity(simp_blkdev_disk, SIMP_BLKDEV_BYTES>>9);
	aml_nand_msg("add_disk(simp_blkdev_disk)");
	add_disk(simp_blkdev_disk);

	return 0;
err_alloc_disk:
	blk_cleanup_queue(simp_blkdev_queue);
err_init_queue:
	return ret;
}

static void  simp_blkdev_exit(void)
{
	del_gendisk(simp_blkdev_disk);
	put_disk(simp_blkdev_disk);
	blk_cleanup_queue(simp_blkdev_queue);
}


static void simp_blkdev_do_request(struct request_queue *q)
{
	struct request *req;
	/* while ((req = elv_next_request(q)) != NULL) { */
	/*
	if ((req->sector + req->current_nr_sectors) << 9 > SIMP_BLKDEV_BYTES) {
	*/
	/* printk(KERN_ERR SIMP_BLKDEV_DISKNAME */
	/* ": bad request: block=%llu, count=%u\n", */
	/* (unsigned long long)req->sector, */
	/* req->current_nr_sectors); */
	/* end_request(req, 0); */
	/* continue; */
	/* } */
	/*  */
	/* switch (rq_data_dir(req)) { */
	/* case READ: */
	/* memcpy(req->buffer,
		simp_blkdev_data + (req->sector << 9),
		req->current_nr_sectors << 9); */
	/* end_request(req, 1); */
	/* break; */
	/* case WRITE: */
	/* memcpy(simp_blkdev_data + (req->sector << 9),
		req->buffer,
		req->current_nr_sectors << 9); */
	/* end_request(req, 1); */
	/* break; */
	/* default: */
	/* /* No default because rq_data_dir(req) is 1 bit */ */
	/* break; */
	/* } */
	/* } */
	while (1)
		aml_nand_msg("add_ntd_blktrans_dev first");

}


