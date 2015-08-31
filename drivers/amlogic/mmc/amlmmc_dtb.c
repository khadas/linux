/*
 * drivers/amlogic/amlnf/dev/amlnf_dtb.c
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

#include <linux/mmc/emmc_partitions.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#define DTB_NAME	"dtb"
#define	SZ_1M	0x00100000
#define		MMC_DTB_PART_OFFSET		(40*SZ_1M)
#define		EMMC_BLOCK_SIZE		(0x100)
#define		MAX_EMMC_BLOCK_SIZE		(128*1024)

static dev_t amlmmc_dtb_no;
struct cdev amlmmc_dtb;
struct device *dtb_dev = NULL;
struct class *amlmmc_dtb_class = NULL;
struct mmc_card *card_dtb = NULL;

int amlmmc_dtb_write(struct mmc_card *card,
		unsigned char *buf, int len)
{
	int ret = 0, start_blk, size, blk_cnt;
	int bit = card->csd.read_blkbits;
	unsigned char *src = NULL;

	if (len > CONFIG_DTB_SIZE) {
		pr_err("%s dtb data len too much", __func__);
		return -EFAULT;
	}
	start_blk = MMC_DTB_PART_OFFSET;
	if (start_blk < 0) {
		ret = -EINVAL;
		return ret;
	}
	start_blk >>= bit;
	size = CONFIG_DTB_SIZE;
	blk_cnt = size>>bit;
	src = (unsigned char *)buf;
	do {
		ret = mmc_write_internal(card, start_blk, EMMC_BLOCK_SIZE, src);
		if (ret) {
			pr_err("%s: save dtb error", __func__);
			ret = -EFAULT;
			return ret;
		}
		start_blk += EMMC_BLOCK_SIZE;
		blk_cnt -= EMMC_BLOCK_SIZE;
		src = (unsigned char *)buf + MAX_EMMC_BLOCK_SIZE;
	} while (blk_cnt != 0);

	return ret;
}

int amlmmc_dtb_read(struct mmc_card *card,
		unsigned char *buf, int len)
{
	int ret = 0, start_blk, size, blk_cnt;
	int bit = card->csd.read_blkbits;
	unsigned char *dst = NULL;

	if (len > CONFIG_DTB_SIZE) {
		pr_err("%s dtb data len too much", __func__);
		return -EFAULT;
	}
	memset(buf, 0x0, len);

	start_blk = MMC_DTB_PART_OFFSET;
	if (start_blk < 0) {
		ret = -EINVAL;
		return ret;
	}

	start_blk >>= bit;
	size = CONFIG_DTB_SIZE;
	blk_cnt = size>>bit;
	dst = (unsigned char *)buf;
	do {
		ret = mmc_read_internal(card, start_blk, EMMC_BLOCK_SIZE, dst);
		if (ret) {
			pr_err("%s read dtb error", __func__);
			ret = -EFAULT;
			return ret;
		}
		start_blk += EMMC_BLOCK_SIZE;
		blk_cnt -= EMMC_BLOCK_SIZE;
		dst = (unsigned char *)buf + MAX_EMMC_BLOCK_SIZE;
	} while (blk_cnt != 0);
	return ret;
}
static CLASS_ATTR(emmcdtb, S_IWUSR | S_IRUGO, NULL, NULL);

int mmc_dtb_open(struct inode *node, struct file *file)
{
	return 0;
}

ssize_t mmc_dtb_read(struct file *file,
		char __user *buf,
		size_t count,
		loff_t *ppos)
{
	unsigned char *dtb_ptr = NULL;
	ssize_t read_size = 0;
	int ret = 0;

	if (*ppos == CONFIG_DTB_SIZE)
		return 0;

	if (*ppos >= CONFIG_DTB_SIZE) {
		pr_err("%s: out of space!", __func__);
		return -EFAULT;
	}

	dtb_ptr = vmalloc(CONFIG_DTB_SIZE);
	if (dtb_ptr == NULL) {
		pr_err("%s: malloc buf failed", __func__);
		return -ENOMEM;
	}

	mmc_claim_host(card_dtb->host);
	ret = amlmmc_dtb_read(card_dtb,
			(unsigned char *)dtb_ptr,
			CONFIG_DTB_SIZE);
	if (ret) {
		pr_err("%s: read failed:%d", __func__, ret);
		ret = -EFAULT;
		goto exit;
	}
	if ((*ppos + count) > CONFIG_DTB_SIZE)
		read_size = CONFIG_DTB_SIZE - *ppos;
	else
		read_size = count;
	ret = copy_to_user(buf, (dtb_ptr + *ppos), read_size);
	*ppos += read_size;
exit:
	mmc_release_host(card_dtb->host);
	vfree(dtb_ptr);
	return read_size;
}

ssize_t mmc_dtb_write(struct file *file,
		const char __user *buf,
		size_t count, loff_t *ppos)
{
	unsigned char *dtb_ptr = NULL;
	ssize_t write_size = 0;
	int ret = 0;

	if (*ppos == CONFIG_DTB_SIZE)
		return 0;

	if (*ppos >= CONFIG_DTB_SIZE) {
		pr_err("%s: out of space!", __func__);
		return -EFAULT;
	}
	dtb_ptr = vmalloc(CONFIG_DTB_SIZE);
	if (dtb_ptr == NULL) {
		pr_err("%s: malloc buf failed", __func__);
		return -ENOMEM;
	}
	mmc_claim_host(card_dtb->host);

	if ((*ppos + count) > CONFIG_DTB_SIZE)
		write_size = CONFIG_DTB_SIZE - *ppos;
	else
		write_size = count;

	ret = copy_from_user((dtb_ptr + *ppos), buf, write_size);

	ret = amlmmc_dtb_write(card_dtb,
			dtb_ptr, CONFIG_DTB_SIZE);
	if (ret) {
		pr_err("%s: write dtb failed", __func__);
		ret = -EFAULT;
		goto exit;
	}

	*ppos += write_size;
exit:
	mmc_release_host(card_dtb->host);
	/* kfree(dtb_ptr); */
	vfree(dtb_ptr);
	return write_size;
}

long mmc_dtb_ioctl(struct file *file, unsigned int cmd, unsigned long args)
{
	return 0;
}

static const struct file_operations dtb_ops = {
	.open = mmc_dtb_open,
	.read = mmc_dtb_read,
	.write = mmc_dtb_write,
	.unlocked_ioctl = mmc_dtb_ioctl,
};

int amlmmc_dtb_init(struct mmc_card *card)
{
	int ret = 0;
	card_dtb = card;
	pr_info("%s: register dtb chardev", __func__);
	ret = alloc_chrdev_region(&amlmmc_dtb_no, 0, 1, DTB_NAME);
	if (ret < 0) {
		pr_err("alloc dtb dev_t no failed");
		ret = -1;
		goto exit_err;
	}

	cdev_init(&amlmmc_dtb, &dtb_ops);
	amlmmc_dtb.owner = THIS_MODULE;
	ret = cdev_add(&amlmmc_dtb, amlmmc_dtb_no, 1);
	if (ret) {
		pr_err("dtb dev add failed");
		ret = -1;
		goto exit_err1;
	}

	amlmmc_dtb_class = class_create(THIS_MODULE, DTB_NAME);
	if (IS_ERR(amlmmc_dtb_class)) {
		pr_err("dtb dev add failed");
		ret = -1;
		goto exit_err2;
	}

	ret = class_create_file(amlmmc_dtb_class, &class_attr_emmcdtb);
	if (ret) {
		pr_err("dtb dev add failed");
		ret = -1;
		goto exit_err2;
	}

	dtb_dev = device_create(amlmmc_dtb_class,
			NULL,
			amlmmc_dtb_no,
			NULL,
			DTB_NAME);
	if (IS_ERR(dtb_dev)) {
		pr_err("dtb dev add failed");
		ret = -1;
		goto exit_err3;
	}

	pr_info("%s: register dtb chardev OK", __func__);

	return ret;

exit_err3:
	class_remove_file(amlmmc_dtb_class, &class_attr_emmcdtb);
	class_destroy(amlmmc_dtb_class);
exit_err2:
	cdev_del(&amlmmc_dtb);
exit_err1:
	unregister_chrdev_region(amlmmc_dtb_no, 1);
exit_err:
	return ret;
}

