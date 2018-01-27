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

/*
 * 2 copies dtb were stored in dtb area.
 * each is 256K.
 * timestamp&checksum are in the tail.
 * |<--------------DTB Area-------------->|
 * |<------DTB1------->|<------DTB2------>|
 */

#define	DTB_GLB_OFFSET		(40*SZ_1M)
#define	DTB_BLK_SIZE		(0x200)
#define	DTB_BLK_CNT			(512)
#define	DTB_SIZE			(DTB_BLK_CNT * DTB_BLK_SIZE)
#define DTB_COPIES			(2)
#define DTB_AREA_BLK_CNT	(DTB_BLK_CNT * DTB_COPIES)
/* pertransfer for internal opearations. */
#define MAX_TRANS_BLK		(256)
#define	MAX_TRANS_SIZE		(MAX_TRANS_BLK * DTB_BLK_SIZE)
#define stamp_after(a, b)   ((int)(b) - (int)(a)  < 0)
struct aml_dtb_rsv {
	u8 data[DTB_BLK_SIZE*DTB_BLK_CNT - 4*sizeof(unsigned int)];
	unsigned int magic;
	unsigned int version;
	unsigned int timestamp;
	unsigned int checksum;
};

struct aml_dtb_info {
	unsigned int stamp[2];
	u8 valid[2];
};

static dev_t amlmmc_dtb_no;
struct cdev amlmmc_dtb;
struct device *dtb_dev = NULL;
struct class *amlmmc_dtb_class = NULL;
struct mmc_card *card_dtb = NULL;
static struct aml_dtb_info dtb_infos = {{0, 0}, {0, 0} };



/* dtb read&write operation with backup updates */
static unsigned int _calc_dtb_checksum(struct aml_dtb_rsv *dtb)
{
	int i = 0;
	int size = sizeof(struct aml_dtb_rsv) - sizeof(unsigned int);
	unsigned int *buffer;
	unsigned int checksum = 0;

	if ((u64)dtb % 4 != 0)
		BUG();

	size = size >> 2;
	buffer = (unsigned int *) dtb;
	while (i < size)
		checksum += buffer[i++];

	return checksum;
}

static int _verify_dtb_checksum(struct aml_dtb_rsv *dtb)
{
	unsigned int checksum;

	checksum = _calc_dtb_checksum(dtb);
	pr_info("calc %x, store %x\n", checksum, dtb->checksum);

	return !(checksum == dtb->checksum);
}

static int _dtb_write(struct mmc_card *mmc,
				int blk, unsigned char *buf)
{
	int ret = 0;
	unsigned char *src = NULL;
	int bit = mmc->csd.read_blkbits;
	int cnt = CONFIG_DTB_SIZE >> bit;

	src = (unsigned char *)buf;

	mmc_claim_host(mmc->host);
	do {
		ret = mmc_write_internal(mmc, blk, MAX_TRANS_BLK, src);
		if (ret) {
			pr_err("%s: save dtb error", __func__);
			ret = -EFAULT;
			break;
		}
		blk += MAX_TRANS_BLK;
		cnt -= MAX_TRANS_BLK;
		src = (unsigned char *)buf + MAX_TRANS_SIZE;
	} while (cnt != 0);
	mmc_release_host(mmc->host);

	return ret;
}


static int _dtb_read(struct mmc_card *mmc,
				int blk, unsigned char *buf)
{
	int ret = 0;
	unsigned char *dst = NULL;
	int bit = mmc->csd.read_blkbits;
	int cnt = CONFIG_DTB_SIZE >> bit;

	dst = (unsigned char *)buf;
	mmc_claim_host(mmc->host);
	do {
		ret = mmc_read_internal(mmc, blk, MAX_TRANS_BLK, dst);
		if (ret) {
			pr_err("%s: save dtb error", __func__);
			ret = -EFAULT;
			break;
		}
		blk += MAX_TRANS_BLK;
		cnt -= MAX_TRANS_BLK;
		dst = (unsigned char *)buf + MAX_TRANS_SIZE;
	} while (cnt != 0);
	mmc_release_host(mmc->host);
	return ret;
}

static int _dtb_init(struct mmc_card *mmc)
{
	int ret = 0;
	struct aml_dtb_rsv *dtb;
	struct aml_dtb_info *info = &dtb_infos;
	int cpy = 1, valid = 0;
	int bit = mmc->csd.read_blkbits;
	int blk;

	/* malloc a buffer. */
	dtb = vmalloc(CONFIG_DTB_SIZE);
	if (dtb == NULL) {
		pr_err("%s: malloc buf failed", __func__);
		return -ENOMEM;
	}

	/* read dtb2 1st, for compatibility without checksum. */
	while (cpy >= 0) {
		blk = (DTB_GLB_OFFSET >> bit) + cpy * DTB_BLK_CNT;
		if (_dtb_read(mmc, blk, (unsigned char *)dtb)) {
			pr_err("%s: block # %#x ERROR!\n",
					__func__, blk);
		} else {
			ret = _verify_dtb_checksum(dtb);
			if (!ret) {
				info->stamp[cpy] = dtb->timestamp;
				info->valid[cpy] = 1;
			} else
				pr_err("cpy %d is not valid\n", cpy);
		}
		valid += info->valid[cpy];
		cpy--;
	}
	pr_info("total valid %d\n", valid);

	vfree(dtb);

	return ret;
}

int amlmmc_dtb_write(struct mmc_card *mmc,
		unsigned char *buf, int len)
{
	int ret = 0, blk;
	int bit = mmc->csd.read_blkbits;
	int cpy, valid;
	struct aml_dtb_rsv *dtb = (struct aml_dtb_rsv *) buf;
	struct aml_dtb_info *info = &dtb_infos;

	if (len > CONFIG_DTB_SIZE) {
		pr_err("%s dtb data len too much", __func__);
		return -EFAULT;
	}
	/* set info */
	valid = info->valid[0] + info->valid[1];
	if (0 == valid)
		dtb->timestamp = 0;
	else if (1 == valid) {
		dtb->timestamp = 1 + info->stamp[info->valid[0]?0:1];
	} else {
		/* both are valid */
		if (info->stamp[0] != info->stamp[1]) {
			pr_info("timestamp are not same %d:%d\n",
				info->stamp[0], info->stamp[1]);
			dtb->timestamp = 1 +
				stamp_after(info->stamp[1], info->stamp[0]) ?
				info->stamp[1]:info->stamp[0];
		} else
			dtb->timestamp = 1 + info->stamp[0];
	}
	/*setting version and magic*/
	dtb->version = 1; /* base version */
	dtb->magic = 0x00447e41; /*A~D\0*/
	dtb->checksum = _calc_dtb_checksum(dtb);
	pr_info("stamp %d, checksum 0x%x, version %d, magic %s\n",
		dtb->timestamp, dtb->checksum,
		dtb->version, (char *)&dtb->magic);
	/* write down... */
	for (cpy = 0; cpy < DTB_COPIES; cpy++) {
		blk = (DTB_GLB_OFFSET >> bit) + cpy * DTB_BLK_CNT;
		ret |= _dtb_write(mmc, blk, buf);
	}

	return ret;
}

/* only read the 1st one. */
int amlmmc_dtb_read(struct mmc_card *mmc,
		unsigned char *buf, int len)
{
	int ret = 0, blk;
	int bit = mmc->csd.read_blkbits;

	if (len > CONFIG_DTB_SIZE) {
		pr_err("%s dtb data len too much", __func__);
		return -EFAULT;
	}

	blk = DTB_GLB_OFFSET >> bit;
	if (blk < 0) {
		ret = -EINVAL;
		return ret;
	}
	ret = _dtb_read(mmc, blk, buf);

	return ret;
}

static ssize_t emmc_dtb_show(struct class *class,
		struct class_attribute *attr, char *buf)
{
	struct aml_dtb_info *i = &dtb_infos;
	ssize_t n;

	n = sprintf(buf, "dtb1: %d, %d\n", i->stamp[0], i->valid[0]);
	n += sprintf(buf+n, "dtb2: %d, %d\n", i->stamp[1], i->valid[1]);
	return n;
}

static CLASS_ATTR(emmcdtb, S_IWUSR | S_IRUGO, emmc_dtb_show, NULL);

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

	/* mmc_claim_host(card_dtb->host); */
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
	/* mmc_release_host(card_dtb->host); */
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
	/* mmc_claim_host(card_dtb->host); */

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
	/* mmc_release_host(card_dtb->host); */
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

	/*fixme, do not check return for */
	_dtb_init(card);

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

