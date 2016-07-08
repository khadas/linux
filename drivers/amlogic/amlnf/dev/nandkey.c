/*
 * drivers/amlogic/amlnf/dev/nandkey.c
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
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/crc32.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/amlogic/key_manage.h>

/* #define KEYSIZE (CONFIG_KEYSIZE - (sizeof(uint))) */

static struct amlnand_chip *aml_chip_key;

int aml_nand_update_key(struct amlnand_chip *aml_chip, char *key_ptr)
{
	int ret = 0;
	int malloc_flag = 0;
	char *key_buf = NULL;

	if (key_buf == NULL) {
		key_buf = kzalloc(aml_chip->keysize, GFP_KERNEL);
		malloc_flag = 1;
		if (key_buf == NULL)
			return -ENOMEM;
		memset(key_buf, 0, aml_chip->keysize);
		ret = amlnand_read_info_by_name(aml_chip,
			(unsigned char *)&(aml_chip->nand_key),
			key_buf,
			KEY_INFO_HEAD_MAGIC,
			aml_chip->keysize);
		if (ret) {
			aml_nand_msg("read key error,%s\n", __func__);
			ret = -EFAULT;
			goto exit;
		}
	} else
		key_buf = key_ptr;

	aml_nand_msg("aml_chip->nand_key : ");
	aml_nand_msg("arg_t %d valid %d,update_flag %d,blkaddr %d,pageaddr %d",
		aml_chip->nand_key.arg_type,
		aml_chip->nand_key.arg_valid,
		aml_chip->nand_key.update_flag,
		aml_chip->nand_key.valid_blk_addr,
		aml_chip->nand_key.valid_page_addr);

	ret = amlnand_save_info_by_name(aml_chip,
		(unsigned char *)&(aml_chip->nand_key),
		key_buf,
		KEY_INFO_HEAD_MAGIC,
		aml_chip->keysize);
	if (ret < 0)
		aml_nand_msg("aml_nand_update_key : save key info failed");

exit:
	if (malloc_flag && (key_buf)) {
		kfree(key_buf);
		key_buf = NULL;
	}
	return 0;
}

/*
 * This funcion reads the u-boot keyionment variables.
 * The f_pos points directly to the key location.
 */
/*
static int32_t nand_key_read(struct aml_keybox_provider_s *provider,
		uint8_t *buf,
		int len,
		int flags)
{

	struct amlnand_chip *aml_chip = provider->priv;
*/
int32_t nand_key_read(uint8_t *buf,
		uint32_t len, uint32_t *actual_lenth)
{
	struct nand_menson_key *key_ptr = NULL;
	struct amlnand_chip *aml_chip = aml_chip_key;
	u32 keysize = aml_chip->keysize - sizeof(u32);
	int error = 0;

	*actual_lenth = keysize;

	if (len > keysize) {
		/*
		No return here! keep consistent, should memset zero
		for the rest.
		*/
		aml_nand_msg("key data len too much,%s", __func__);
		memset(buf + keysize, 0 , len - keysize);
	}

	key_ptr = kzalloc(aml_chip->keysize, GFP_KERNEL);
	if (key_ptr == NULL)
		return -ENOMEM;

	amlnand_get_device(aml_chip, CHIP_READING);
	error = amlnand_read_info_by_name(aml_chip,
		(unsigned char *)&(aml_chip->nand_key),
		(unsigned char *)key_ptr,
		KEY_INFO_HEAD_MAGIC,
		aml_chip->keysize);
	if (error) {
		aml_nand_msg("read key error,%s", __func__);
		error = -EFAULT;
		goto exit;
	}

	memcpy(buf, key_ptr->data, keysize);
exit:
	amlnand_release_device(aml_chip);
	kfree(key_ptr);
	return 0;
}
EXPORT_SYMBOL(nand_key_read);

/*
static int32_t nand_key_write(struct aml_keybox_provider_s *provider,
		uint8_t *buf,
		int len)
{
	struct amlnand_chip *aml_chip = provider->priv;
*/

int32_t nand_key_write(uint8_t *buf,
		uint32_t len, uint32_t *actual_lenth)
{
	struct nand_menson_key *key_ptr = NULL;
	struct amlnand_chip *aml_chip = aml_chip_key;
	u32 keysize = aml_chip->keysize - sizeof(u32);
	int error = 0;

	if (len > keysize) {
		/*
		No return here! keep consistent, should memset zero
		for the rest.
		*/
		aml_nand_msg("key data len too much,%s", __func__);
		memset(buf + keysize, 0 , len - keysize);
	}

	key_ptr = kzalloc(aml_chip->keysize, GFP_KERNEL);
	if (key_ptr == NULL)
		return -ENOMEM;

	memcpy(key_ptr->data + 0, buf, keysize);
	amlnand_get_device(aml_chip, CHIP_WRITING);

	error = amlnand_save_info_by_name(aml_chip,
		(unsigned char *) &(aml_chip->nand_key),
		(unsigned char *)key_ptr,
		KEY_INFO_HEAD_MAGIC,
		aml_chip->keysize);
	if (error) {
		aml_nand_msg("save key error,%s", __func__);
		error = -EFAULT;
		goto exit;
	}
exit:
	amlnand_release_device(aml_chip);
	kfree(key_ptr);
	return error;
}
EXPORT_SYMBOL(nand_key_write);

/*
static struct aml_keybox_provider_s nand_provider = {
	.name = "nand_key",
	.read = nand_key_read,
	.write = nand_key_write,
};
*/

int aml_key_init(struct amlnand_chip *aml_chip)
{
	int ret = 0;
	struct nand_menson_key *key_ptr = NULL;
/*
	struct aml_keybox_provider_s *provider;
*/
	key_ptr = aml_nand_malloc(aml_chip->keysize);
	if (key_ptr == NULL) {
		aml_nand_msg("nand malloc for key_ptr failed");
		ret = -1;
		goto exit_error0;
	}
	memset(key_ptr, 0x0, aml_chip->keysize);
	aml_nand_dbg("nand key: nand_key_probe. ");

	ret = amlnand_info_init(aml_chip,
		(unsigned char *)&(aml_chip->nand_key),
		(unsigned char *)key_ptr,
		KEY_INFO_HEAD_MAGIC,
		aml_chip->keysize);
	if (ret < 0)
		aml_nand_msg("invalid nand key\n");

	aml_chip_key = aml_chip;

	storage_ops_read(nand_key_read);
	storage_ops_write(nand_key_write);
/*
	nand_provider.priv = aml_chip_key;

	provider = aml_keybox_provider_get(nand_provider.name);
	if (provider)
		return ret;

	ret = aml_keybox_provider_register(&nand_provider);
	if (ret)
		BUG();
*/

exit_error0:
	if (key_ptr) {
		aml_nand_free(key_ptr);
		key_ptr = NULL;
	}
	return ret;
}


int aml_key_reinit(struct amlnand_chip *aml_chip)
{
	int ret = 0;
	struct nand_menson_key *key_ptr = NULL;

	key_ptr = vmalloc(aml_chip->keysize);
	if (key_ptr == NULL) {
		aml_nand_msg("nand malloc for key_ptr failed");
		ret = -1;
		goto exit_error0;
	}
	memset(key_ptr, 0x0, aml_chip->keysize);
	aml_nand_dbg("nand key: nand_key_probe. ");

	amlnand_get_device(aml_chip, CHIP_READING);
	ret = amlnand_info_init(aml_chip,
		(unsigned char *)&(aml_chip->nand_key),
		(unsigned char *)key_ptr,
		KEY_INFO_HEAD_MAGIC,
		aml_chip->keysize);
	if (ret < 0)
		aml_nand_msg("invalid nand key\n");
	amlnand_release_device(aml_chip);
exit_error0:
	if (key_ptr) {
		vfree(key_ptr);
		key_ptr = NULL;
	}
	return ret;
}

