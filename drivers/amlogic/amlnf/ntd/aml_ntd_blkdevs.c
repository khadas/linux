/*
 * drivers/amlogic/amlnf/ntd/aml_ntd_blkdevs.c
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
#include <linux/fd.h>
#include <linux/param.h>

#include <mtd/mtd-abi.h>
#include "aml_ntd.h"

LIST_HEAD(ntd_blktrans_majors);
static DEFINE_MUTEX(blktrans_ref_mutex);

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static void blktrans_dev_release(struct kref *kref)
{
	struct ntd_blktrans_dev *dev = NULL;

	dev = container_of(kref, struct ntd_blktrans_dev, ref);

	dev->disk->private_data = NULL;
	blk_cleanup_queue(dev->rq);
	put_disk(dev->disk);
	list_del(&dev->list);
	kfree(dev);
}

int amlnf_class_register(struct class *class)
{
	return class_register(class);
}
EXPORT_SYMBOL(amlnf_class_register);

void amlnf_ktime_get_ts(struct timespec *ts)
{
	ktime_get_ts(ts);
}
EXPORT_SYMBOL(amlnf_ktime_get_ts);
/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static struct ntd_blktrans_dev *blktrans_dev_get(struct gendisk *disk)
{
	struct ntd_blktrans_dev *dev;

	mutex_lock(&blktrans_ref_mutex);
	dev = disk->private_data;

	if (!dev)
		goto unlock;
	kref_get(&dev->ref);
unlock:
	mutex_unlock(&blktrans_ref_mutex);
	return dev;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static void blktrans_dev_put(struct ntd_blktrans_dev *dev)
{
	mutex_lock(&blktrans_ref_mutex);
	kref_put(&dev->ref, blktrans_dev_release);
	mutex_unlock(&blktrans_ref_mutex);
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
#define blk_queue_plugged(q)    test_bit(18, &(q)->queue_flags)
static int ntd_blktrans_thread(void *arg)
{
	struct ntd_blktrans_dev *dev = arg;
	struct ntd_blktrans_ops *tr = dev->tr;
	struct request_queue *rq = dev->rq;
	struct request *req = NULL;
	struct ntd_info *ntd;
	int res;
	int background_done = 0;

	ntd = dev->ntd;
	spin_lock_irq(rq->queue_lock);

	while (!kthread_should_stop()) {
		if (ntd->thread_stop_flag) {
			spin_unlock_irq(rq->queue_lock);
			schedule();
			spin_lock_irq(rq->queue_lock);
			continue;
		}
		dev->bg_stop = false;
		if (!blk_queue_plugged(rq))
			req = blk_fetch_request(rq);
		if (!req) {
			if (tr->background && !background_done) {
				spin_unlock_irq(rq->queue_lock);
				mutex_lock(&dev->lock);
				tr->background(dev);
				mutex_unlock(&dev->lock);
				spin_lock_irq(rq->queue_lock);
				/*
				 * Do background processing just once per idle
				 * period.
				 */
				background_done = !dev->bg_stop;
				continue;
			}
			set_current_state(TASK_INTERRUPTIBLE);

			if (kthread_should_stop())
				set_current_state(TASK_RUNNING);

			spin_unlock_irq(rq->queue_lock);
			schedule();
			spin_lock_irq(rq->queue_lock);
			continue;
		}

		spin_unlock_irq(rq->queue_lock);

		mutex_lock(&dev->lock);
		res = tr->do_blktrans_request(tr, dev, req);
		mutex_unlock(&dev->lock);

		spin_lock_irq(rq->queue_lock);

		if (blk_rq_sectors(req) > 0) {
			__blk_end_request(req, res, 512*blk_rq_sectors(req));
			req = NULL;
		}

		background_done = 0;
	}

	if (req)
		__blk_end_request_all(req, -EIO);

	spin_unlock_irq(rq->queue_lock);

	return 0;
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static void ntd_blktrans_request(struct request_queue *rq)
{
	struct ntd_blktrans_dev *dev;
	struct request *req = NULL;

	dev = rq->queuedata;

	if (!dev)
		while ((req = blk_fetch_request(rq)) != NULL)
			__blk_end_request_all(req, -ENODEV);
	else {
		dev->bg_stop = true;
		wake_up_process(dev->thread);
	}
}

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int blktrans_open(struct block_device *bdev, fmode_t mode)
{
	struct ntd_blktrans_dev *dev = blktrans_dev_get(bdev->bd_disk);
	int ret = 0;

	if (!dev)
		return -ERESTARTSYS; /* FIXME: busy loop! -arnd*/

	mutex_lock(&dev->lock);

	if (dev->open++)
		goto unlock;

	kref_get(&dev->ref);
	__module_get(dev->tr->owner);

	/* if (!dev->ntd) */
	/* goto unlock; */

	if (dev->tr->open) {
		ret = dev->tr->open(dev);
		if (ret)
			goto error_put;
	}

	/* ret = __get_ntd_device(dev->ntd); */
	/* if (ret) */
	/* goto error_release; */

unlock:
	mutex_unlock(&dev->lock);
	blktrans_dev_put(dev);
	return ret;

	/* error_release: */
	/* if (dev->tr->release) */
	/* dev->tr->release(dev); */
error_put:
	module_put(dev->tr->owner);
	kref_put(&dev->ref, blktrans_dev_release);
	mutex_unlock(&dev->lock);
	blktrans_dev_put(dev);
	return ret;
}
EXPORT_SYMBOL(blktrans_open);
/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
void blktrans_release(struct gendisk *disk, fmode_t mode)
{
	struct ntd_blktrans_dev *dev = blktrans_dev_get(disk);
	/* int ret = 0; */

	if (!dev)
		return;

	mutex_lock(&dev->lock);

	if (--dev->open)
		goto unlock;

	kref_put(&dev->ref, blktrans_dev_release);
	module_put(dev->tr->owner);

	/* if (dev->ntd) { */
	/* ret = dev->tr->release ? dev->tr->release(dev) : 0; */
	/* __put_ntd_device(dev->ntd); */
	/* } */
unlock:
	mutex_unlock(&dev->lock);
	blktrans_dev_put(dev);
	return;
}
EXPORT_SYMBOL(blktrans_release);
/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int blktrans_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	struct ntd_blktrans_dev *dev = blktrans_dev_get(bdev->bd_disk);
	int ret = -ENXIO;

	if (!dev)
		return ret;

	mutex_lock(&dev->lock);

	/* if (!dev->ntd) */
	/* goto unlock; */

	ret = dev->tr->getgeo ? dev->tr->getgeo(dev, geo) : 0;
	/* unlock: */
	mutex_unlock(&dev->lock);
	blktrans_dev_put(dev);
	return ret;
}
EXPORT_SYMBOL(blktrans_getgeo);
/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
#define BLKWIPEPART    _IO('M', 127)
int blktrans_ioctl(struct block_device *bdev,
		fmode_t mode,
		unsigned int cmd,
		unsigned long arg)
{
	struct ntd_blktrans_dev *dev = blktrans_dev_get(bdev->bd_disk);
	int ret = -ENXIO;
	struct ntd_blktrans_ops *tr = dev->tr;

	if (!dev)
		return ret;

	mutex_lock(&dev->lock);

	/* if (!dev->ntd) */
	/* goto unlock; */

	switch (cmd) {
	case BLKFLSBUF:
		ret = dev->tr->flush ? dev->tr->flush(dev) : 0;
		aml_nand_msg("blktrans_ioctl nftl_flush");
	break;
	case BLKWIPEPART:
		/* printk("blktrans_ioctl BLKWIPEPART\n"); */
		if (tr->wipe_part) {
			aml_nand_msg("blktrans_ioctl tr->wipe_part :");
			ret = tr->wipe_part(dev);
		}
	break;
	/* case BLKGETSECTS: */
	/* case BLKFREESECTS: */
		/* if (tr->update_blktrans_sysinfo) */
		/* tr->update_blktrans_sysinfo(dev, cmd, arg); */
	/* break; */
	default:
		aml_nand_msg("blktrans_ioctl  -cmd: %x,%lx,%x\n",
			cmd,
			FDGETPRM,
			HZ);
		ret = -ENOTTY;
	break;
	}
	/* unlock: */
	mutex_unlock(&dev->lock);
	blktrans_dev_put(dev);
	return ret;
}
EXPORT_SYMBOL(blktrans_ioctl);
/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static const struct block_device_operations _ntd_blktrans_ops = {
	.owner      = THIS_MODULE,
	.open       = blktrans_open,
	.release    = blktrans_release,
	.ioctl      = blktrans_ioctl,
	.getgeo     = blktrans_getgeo,
};

/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int blk_class_register(struct class *cls)
{
	return class_register(cls);
}
EXPORT_SYMBOL(blk_class_register);
/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int blk_device_register(struct device *dev, int num)
{
	dev->parent = &platform_bus;
	dev_set_name(dev, "aml_nftl_dev%d", num);

	return device_register(dev);
}
EXPORT_SYMBOL(blk_device_register);
/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int add_ntd_blktrans_dev(struct ntd_blktrans_dev *new)
{
	struct ntd_blktrans_ops *tr = new->tr;
	struct ntd_blktrans_dev *d;
	struct gendisk *gd;
	int ret = -ENOMEM;

	if (mutex_trylock(&ntd_table_mutex)) {
		mutex_unlock(&ntd_table_mutex);
		BUG();
	}

	mutex_lock(&blktrans_ref_mutex);

	list_for_each_entry(d, &tr->devs, list) {
		/* printk("add_ntd_blktrans_dev first\n"); */
		if (d->devnum == new->devnum) {
			/* Required number taken */
			mutex_unlock(&blktrans_ref_mutex);
			return -EBUSY;
		} else if (d->devnum > new->devnum) {
			/* Required number was free */
			list_add_tail(&new->list, &d->list);
			goto added;
		}
	}

	if (new->devnum > (MINORMASK >> tr->part_bits)) {
		mutex_unlock(&blktrans_ref_mutex);
		return -EBUSY;
	}

	list_add_tail(&new->list, &tr->devs);

added:
	mutex_unlock(&blktrans_ref_mutex);
	mutex_init(&new->lock);
	kref_init(&new->ref);

	if (!tr->writesect)
		new->readonly = 1;

	/* Create gendisk */
	gd = alloc_disk(1 << tr->part_bits);
	if (!gd)
		goto error2;

	new->disk = gd;
	gd->private_data = new;
	gd->major = tr->major;
	/* gd->major = COMPAQ_SMART2_MAJOR; */
	gd->first_minor = (new->devnum) << tr->part_bits;
	gd->fops = &_ntd_blktrans_ops;
	/* snprintf(gd->disk_name, sizeof(gd->disk_name),"nand%s", new->name);*/
	snprintf(gd->disk_name, sizeof(gd->disk_name), "%s", new->name);
	/* printk("gd->disk_name: %s %d\n",gd->disk_name,new->size); */
	set_capacity(gd, new->size);

	/* Create the request queue */
	spin_lock_init(&new->queue_lock);
	new->rq = blk_init_queue(ntd_blktrans_request, &new->queue_lock);
	if (!new->rq)
		goto error3;

	new->rq->queuedata = new;
	blk_queue_logical_block_size(new->rq, tr->blksize);

	/*
	if (tr->discard) {
		printk("Enable QUEUE_FLAG_DISCARD for NFTL\n");
	*/
	queue_flag_set_unlocked(QUEUE_FLAG_DISCARD, new->rq);
	new->rq->limits.max_discard_sectors = UINT_MAX;
	/*
	}
	*/

	gd->queue = new->rq;

	/* Create processing thread */
	/*
	new->thread = \
	kthread_run(ntd_blktrans_thread, new,"%s%d", tr->name, new->ntd->index);
	*/
	new->thread = kthread_run(ntd_blktrans_thread,
			new,
			"%s%d",
			tr->name,
			new->devnum);
	if (IS_ERR(new->thread)) {
		ret = PTR_ERR(new->thread);
		goto error4;
	}

	/* if(!ntd_dev_register(new)){ */
		/* gd->driverfs_dev = &new->dev; */
	/* } */

	if (new->readonly)
		set_disk_ro(gd, 1);

	/* printk(KERN_WARNING"add_disk %x %x\n",new->devnum,tr->part_bits); */
	add_disk(gd);

	if (new->disk_attributes) {
		ret = sysfs_create_group(&disk_to_dev(gd)->kobj,
			new->disk_attributes);
		WARN_ON(ret);
	}

	return 0;

error4:
	blk_cleanup_queue(new->rq);
error3:
	put_disk(new->disk);
error2:
	list_del(&new->list);
	/* error1: */
	return ret;
}
EXPORT_SYMBOL(add_ntd_blktrans_dev);
/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int del_ntd_blktrans_dev(struct ntd_blktrans_dev *old)
{
	unsigned long flags;

	if (mutex_trylock(&ntd_table_mutex)) {
		mutex_unlock(&ntd_table_mutex);
		BUG();
	}

	if (old->disk_attributes)
		sysfs_remove_group(&disk_to_dev(old->disk)->kobj,
			old->disk_attributes);

	/* Stop new requests to arrive */
	del_gendisk(old->disk);

	/* Stop the thread */
	kthread_stop(old->thread);

	/* Kill current requests */
	spin_lock_irqsave(&old->queue_lock, flags);
	old->rq->queuedata = NULL;
	blk_start_queue(old->rq);
	spin_unlock_irqrestore(&old->queue_lock, flags);

	/* If the device is currently open, tell trans driver to close it,
	then put mtd device, and don't touch it again */
	/* mutex_lock(&old->lock); */
	/* if (old->open) { */
	/* if (old->tr->release) */
	/* old->tr->release(old); */
	/* __put_ntd_device(old->ntd); */
	/* } */
	/*  */
	/* old->ntd = NULL; */
	/*  */
	/* mutex_unlock(&old->lock); */

	blktrans_dev_put(old);
	return 0;
}
EXPORT_SYMBOL(del_ntd_blktrans_dev);
/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int register_ntd_blktrans(struct ntd_blktrans_ops *tr)
{
	struct ntd_info *ntd;
	int ret;

	/* Register the notifier if/when the first device type is
	registered, to prevent the link/init ordering from fucking
	us over. */
	/* if (!blktrans_notifier.list.next){ */
	/* printk("register_ntd_blktrans blktrans_notifier\n"); */
	/* register_ntd_user(&blktrans_notifier); */
	/* } */

	mutex_lock(&ntd_table_mutex);

	ret = register_blkdev(tr->major, tr->name);
	if (ret < 0) {
		aml_nand_msg("Unable to register %s blk device on major %d: %d",
			tr->name,
			tr->major,
			ret);
		mutex_unlock(&ntd_table_mutex);
		return ret;
	}

	if (ret)
		tr->major = ret;

	tr->blkshift = ffs(tr->blksize) - 1;

	INIT_LIST_HEAD(&tr->devs);

	list_add(&tr->list, &ntd_blktrans_majors);

	ntd_for_each_device(ntd) {
		tr->add_ntd(tr, ntd);
	}

	if (!tr->do_blktrans_request)
		aml_nand_msg("do_blktrans_request not found!");

	mutex_unlock(&ntd_table_mutex);
	return 0;
}
EXPORT_SYMBOL(register_ntd_blktrans);
/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
int deregister_ntd_blktrans(struct ntd_blktrans_ops *tr)
{
	struct ntd_blktrans_dev *dev, *next;

	mutex_lock(&ntd_table_mutex);

	/* Remove it from the list of active majors */
	list_del(&tr->list);

	list_for_each_entry_safe(dev, next, &tr->devs, list)
		tr->remove_dev(dev);

	unregister_blkdev(tr->major, tr->name);
	mutex_unlock(&ntd_table_mutex);

	BUG_ON(!list_empty(&tr->devs));
	return 0;
}
EXPORT_SYMBOL(deregister_ntd_blktrans);
/*****************************************************************************
*Name         :
*Description  :
*Parameter    :
*Return       :
*Note         :
*****************************************************************************/
static void __exit ntd_blktrans_exit(void)
{
	/* No race here -- if someone's currently in register_ntd_blktrans
	we're screwed anyway. */
}


MODULE_AUTHOR("David Woodhouse <dwmw2@infradead.org>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Common interface to block layer for NTD translation layer");
