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

#include "../include/phynand.h"
#include<linux/cdev.h>
#include <linux/device.h>



#define DTB_NAME	"dtb"
#define DTB_CLS_NAME	"dtb"
static dev_t amlnf_dtb_no;
struct cdev amlnf_dtb;
struct device *nfdtb_dev = NULL;
struct class *amlnf_dtb_class = NULL;

struct amlnand_chip *aml_chip_dtb = NULL;

int amlnf_dtb_save(unsigned char *buf, int len)
{
	unsigned char *dtb_buf = NULL;
	struct nand_flash *flash = &aml_chip_dtb->flash;
	int ret = 0;

	aml_nand_msg("%s", __func__);

	if (len > aml_chip_dtb->dtbsize) {
		aml_nand_msg("warnning!!! %s dtb data len too much", __func__);
		len = aml_chip_dtb->dtbsize;
		/*return -EFAULT;*/
	}

	dtb_buf = vmalloc(aml_chip_dtb->dtbsize + flash->pagesize);
	if (dtb_buf == NULL) {
		aml_nand_msg("%s malloc failed", __func__);
		ret = -1;
		goto exit_err;
	}
	memset(dtb_buf, 0, aml_chip_dtb->dtbsize);
	memcpy(dtb_buf, buf, len);

	ret = amlnand_save_info_by_name(aml_chip_dtb,
		(unsigned char *)&(aml_chip_dtb->amlnf_dtb),
		dtb_buf,
		DTD_INFO_HEAD_MAGIC,
		aml_chip_dtb->dtbsize);
	if (ret) {
		aml_nand_msg("%s: save dtb error", __func__);
		ret = -EFAULT;
		goto exit_err;
	}
exit_err:
	if (dtb_buf) {
		vfree(dtb_buf);
		dtb_buf = NULL;
	}
	return ret;
}


int amlnf_dtb_read(unsigned char *buf, int len)
{
	unsigned char *dtb_buf = NULL;
	int ret = 0;
	struct nand_flash *flash = &aml_chip_dtb->flash;

	aml_nand_msg("%s", __func__);

	if (len > aml_chip_dtb->dtbsize) {
		aml_nand_msg("warnning!!! %s dtb data len too much", __func__);
		len = aml_chip_dtb->dtbsize;
		/*return -EFAULT;*/
	}

	if (aml_chip_dtb->amlnf_dtb.arg_valid == 0) {
		memset(buf, 0x0, len);
		aml_nand_msg("%s invalid", __func__);
		return 0;
	}

	dtb_buf = vmalloc(aml_chip_dtb->dtbsize + flash->pagesize);
	if (dtb_buf == NULL) {
		aml_nand_msg("%s malloc failed", __func__);
		ret = -1;
		goto exit_err;
	}
	memset(dtb_buf, 0, aml_chip_dtb->dtbsize);

	ret = amlnand_read_info_by_name(aml_chip_dtb,
		(unsigned char *)&(aml_chip_dtb->amlnf_dtb),
		(unsigned char *)dtb_buf,
		DTD_INFO_HEAD_MAGIC,
		aml_chip_dtb->dtbsize);
	if (ret) {
		aml_nand_msg("%s read dtb error", __func__);
		ret = -EFAULT;
		goto exit_err;
	}

	memcpy(buf, dtb_buf, len);
exit_err:
	if (dtb_buf) {
		/* kfree(dtb_buf); */
		vfree(dtb_buf);
		dtb_buf = NULL;
	}
	return ret;
}

ssize_t dtb_show(struct class *class, struct class_attribute *attr,
		char *buf)
{
	aml_nand_dbg("%s", __func__);

	return 0;
}

ssize_t dtb_store(struct class *class, struct class_attribute *attr,
		const char *buf, size_t count)
{
	int ret = 0;
	unsigned char *dtb_ptr = NULL;
	aml_nand_dbg("%s", __func__);

	dtb_ptr = kzalloc(aml_chip_dtb->dtbsize, GFP_KERNEL);
	if (dtb_ptr == NULL) {
		aml_nand_msg("%s: malloc buf failed", __func__);
		return -ENOMEM;
	}

	ret = amlnf_dtb_read(dtb_ptr, aml_chip_dtb->dtbsize);
	if (ret) {
		aml_nand_msg("%s: read failed", __func__);
		kfree(dtb_ptr);
		return -EFAULT;
	}

	ret = amlnf_dtb_save(dtb_ptr, aml_chip_dtb->dtbsize);
	if (ret) {
		aml_nand_msg("%s: save failed", __func__);
		kfree(dtb_ptr);
		return -EFAULT;
	}

	aml_nand_dbg("dtb_store : OK #####");
	return count;
}

static CLASS_ATTR(nfdtb, S_IWUSR | S_IRUGO, dtb_show, dtb_store);

int dtb_open(struct inode *node, struct file *file)
{
	return 0;
}

ssize_t dtb_read(struct file *file,
		char __user *buf,
		size_t count,
		loff_t *ppos)
{
	unsigned char *dtb_ptr = NULL;
	struct nand_flash *flash = &aml_chip_dtb->flash;
	ssize_t read_size = 0;
	int ret = 0;

	if (*ppos == aml_chip_dtb->dtbsize)
		return 0;

	if (*ppos >= aml_chip_dtb->dtbsize) {
		aml_nand_msg("%s: out of space!", __func__);
		return -EFAULT;
	}

	dtb_ptr = vmalloc(aml_chip_dtb->dtbsize + flash->pagesize);
	if (dtb_ptr == NULL) {
		aml_nand_msg("%s: malloc buf failed", __func__);
		return -ENOMEM;
	}

	amlnand_get_device(aml_chip_dtb, CHIP_READING);
	ret = amlnf_dtb_read((unsigned char *)dtb_ptr, aml_chip_dtb->dtbsize);
	if (ret) {
		aml_nand_msg("%s: read failed:%d", __func__, ret);
		ret = -EFAULT;
		goto exit;
	}

	if ((*ppos + count) > aml_chip_dtb->dtbsize)
		read_size = aml_chip_dtb->dtbsize - *ppos;
	else
		read_size = count;

	ret = copy_to_user(buf, (dtb_ptr + *ppos), read_size);
	*ppos += read_size;
exit:
	amlnand_release_device(aml_chip_dtb);
	vfree(dtb_ptr);
	return read_size;
}

ssize_t dtb_write(struct file *file,
		const char __user *buf,
		size_t count, loff_t *ppos)
{
	unsigned char *dtb_ptr = NULL;
	ssize_t write_size = 0;
	struct nand_flash *flash = &aml_chip_dtb->flash;
	int ret = 0;

	if (*ppos == aml_chip_dtb->dtbsize)
		return 0;

	if (*ppos >= aml_chip_dtb->dtbsize) {
		aml_nand_msg("%s: out of space!", __func__);
		return -EFAULT;
	}

	dtb_ptr = vmalloc(aml_chip_dtb->dtbsize + flash->pagesize);
	if (dtb_ptr == NULL) {
		aml_nand_msg("%s: malloc buf failed", __func__);
		return -ENOMEM;
	}
	amlnand_get_device(aml_chip_dtb, CHIP_WRITING);

	ret = amlnf_dtb_read((unsigned char *)dtb_ptr, aml_chip_dtb->dtbsize);
	if (ret) {
		aml_nand_msg("%s: read failed", __func__);
		ret = -EFAULT;
		goto exit;
	}

	if ((*ppos + count) > aml_chip_dtb->dtbsize)
		write_size = aml_chip_dtb->dtbsize - *ppos;
	else
		write_size = count;

	ret = copy_from_user((dtb_ptr + *ppos), buf, write_size);

	ret = amlnf_dtb_save(dtb_ptr, aml_chip_dtb->dtbsize);
	if (ret) {
		aml_nand_msg("%s: read failed", __func__);
		ret = -EFAULT;
		goto exit;
	}

	*ppos += write_size;
exit:
	amlnand_release_device(aml_chip_dtb);
	/* kfree(dtb_ptr); */
	vfree(dtb_ptr);
	return write_size;
}

long dtb_ioctl(struct file *file, unsigned int cmd, unsigned long args)
{
	return 0;
}

static const struct file_operations dtb_ops = {
	.open = dtb_open,
	.read = dtb_read,
	.write = dtb_write,
	.unlocked_ioctl = dtb_ioctl,
};

int amlnf_dtb_init(struct amlnand_chip *aml_chip)
{
	int ret = 0;
	unsigned char *dtb_buf = NULL;
	aml_chip_dtb = aml_chip;

	dtb_buf = aml_nand_malloc(aml_chip_dtb->dtbsize);
	if (dtb_buf == NULL) {
		aml_nand_msg("%s: malloc failed", __func__);
		ret = -1;
		goto exit_err;
	}
	memset(dtb_buf, 0x0, aml_chip_dtb->dtbsize);

	ret = amlnand_info_init(aml_chip,
		(unsigned char *)&(aml_chip->amlnf_dtb),
		dtb_buf,
		DTD_INFO_HEAD_MAGIC,
		aml_chip_dtb->dtbsize);
	if (ret < 0) {
		aml_nand_msg("%s init failed\n", __func__);
		/*
		ret = -1;
		goto exit_err;
		*/
	}

	aml_nand_dbg("%s: register dtb chardev", __func__);
	ret = alloc_chrdev_region(&amlnf_dtb_no, 0, 1, DTB_NAME);
	if (ret < 0) {
		aml_nand_msg("alloc dtb dev_t no failed");
		ret = -1;
		goto exit_err;
	}

	cdev_init(&amlnf_dtb, &dtb_ops);
	amlnf_dtb.owner = THIS_MODULE;
	ret = cdev_add(&amlnf_dtb, amlnf_dtb_no, 1);
	if (ret) {
		aml_nand_msg("dtb dev add failed");
		ret = -1;
		goto exit_err1;
	}


	amlnf_dtb_class = class_create(THIS_MODULE, DTB_CLS_NAME);
	if (IS_ERR(amlnf_dtb_class)) {
		aml_nand_msg("dtb dev add failed");
		ret = -1;
		goto exit_err2;
	}

	ret = class_create_file(amlnf_dtb_class, &class_attr_nfdtb);
	if (ret) {
		aml_nand_msg("dtb dev add failed");
		ret = -1;
		goto exit_err2;
	}

	nfdtb_dev = device_create(amlnf_dtb_class,
		NULL,
		amlnf_dtb_no,
		NULL,
		DTB_NAME);
	if (IS_ERR(nfdtb_dev)) {
		aml_nand_msg("dtb dev add failed");
		ret = -1;
		goto exit_err3;
	}

	aml_nand_dbg("%s: register dtb chardev OK", __func__);

	kfree(dtb_buf);
	dtb_buf = NULL;

	return ret;

exit_err3:
	class_remove_file(amlnf_dtb_class, &class_attr_nfdtb);
	class_destroy(amlnf_dtb_class);
exit_err2:
	cdev_del(&amlnf_dtb);
exit_err1:
	unregister_chrdev_region(amlnf_dtb_no, 1);
exit_err:
	kfree(dtb_buf);
	dtb_buf = NULL;
	return ret;
}

int amlnf_dtb_reinit(struct amlnand_chip *aml_chip)
{
	int ret = 0;
	unsigned char *dtb_buf = NULL;
	aml_chip_dtb = aml_chip;

	dtb_buf = vmalloc(aml_chip_dtb->dtbsize);
	if (dtb_buf == NULL) {
		aml_nand_msg("%s: malloc failed", __func__);
		ret = -1;
		goto exit_err;
	}
	memset(dtb_buf, 0x0, aml_chip_dtb->dtbsize);
	amlnand_get_device(aml_chip, CHIP_READING);
	ret = amlnand_info_init(aml_chip,
		(unsigned char *)&(aml_chip->amlnf_dtb),
		dtb_buf,
		DTD_INFO_HEAD_MAGIC,
		aml_chip_dtb->dtbsize);
	if (ret < 0)
		aml_nand_msg("%s init failed\n", __func__);
	amlnand_release_device(aml_chip);
exit_err:
	vfree(dtb_buf);
	dtb_buf = NULL;
	return ret;
}

int aml_nand_update_dtb(struct amlnand_chip *aml_chip, char *dtb_ptr)
{
	int ret = 0;
	char malloc_flag = 0;
	char *dtb_buf = NULL;

	if (dtb_buf == NULL) {
		dtb_buf = kzalloc(aml_chip_dtb->dtbsize, GFP_KERNEL);
		malloc_flag = 1;
		if (dtb_buf == NULL)
			return -ENOMEM;
		memset(dtb_buf, 0, aml_chip_dtb->dtbsize);
		ret = amlnand_read_info_by_name(aml_chip,
			(unsigned char *)&(aml_chip->amlnf_dtb),
			dtb_buf,
			DTD_INFO_HEAD_MAGIC,
			aml_chip_dtb->dtbsize);
		if (ret) {
			aml_nand_msg("read dtb error,%s\n", __func__);
			ret = -EFAULT;
			goto exit;
		}
	} else
		dtb_buf = dtb_ptr;

	ret = amlnand_save_info_by_name(aml_chip,
		(unsigned char *)&(aml_chip->amlnf_dtb),
		dtb_buf,
		DTD_INFO_HEAD_MAGIC,
		aml_chip_dtb->dtbsize);
	if (ret < 0)
		aml_nand_msg("%s: update failed", __func__);
exit:
	if (malloc_flag && (dtb_buf)) {
		kfree(dtb_buf);
		dtb_buf = NULL;
	}
	return 0;
}


