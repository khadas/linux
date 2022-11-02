// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#include <linux/property.h>
#include <linux/mm.h>
#include <linux/platform_device.h>
#ifdef CONFIG_AMLOGIC_EFUSE
#include <linux/amlogic/efuse.h>
#endif
#include <linux/amlogic/key_manage.h>
#include "unifykey.h"
#include "amlkey_if.h"
#include "security_key.h"
#include "normal_key.h"

#define SECURESTORAGE_HEAD_SIZE		(256)
#define SECURESTORAGE_WHOLE_SIZE		(0x40000)

#undef pr_fmt
#define pr_fmt(fmt) "unifykey: " fmt

#define SECUREKEY_SIZE		SECURESTORAGE_WHOLE_SIZE

static DEFINE_MUTEX(securekey_lock);
u8 *securekey_prebuf;

struct storagekey_info_t {
	u8  *buffer;
	u32 size;
};

static struct storagekey_info_t storagekey_info = {
	.buffer = NULL,
	/* default size */
	.size = SECURESTORAGE_WHOLE_SIZE,
};

static struct unifykey_storage_ops ops;
static struct unifykey_type unifykey_types = {
	.ops = &ops,
};

static inline
int is_valid_unifykey_storage_type(u32 storage_type)
{
	if (storage_type == UNIFYKEY_STORAGE_TYPE_INVALID ||
	    storage_type >= UNIFYKEY_STORAGE_TYPE_MAX) {
		return 0;
	}

	return -EINVAL;
}

int register_unifykey_types(struct unifykey_type *uk_type)
{
	u32 type;

	if (!uk_type) {
		pr_err("the uk_type is NULL\n");
		return -EINVAL;
	}
	type = uk_type->storage_type;

	if (!is_valid_unifykey_storage_type(type)) {
		pr_err("not a supported unifykey storage type\n");
		return -EINVAL;
	}

	if (is_valid_unifykey_storage_type(unifykey_types.storage_type)) {
		pr_err("already registered\n");
		return -EBUSY;
	}

	unifykey_types.storage_type = uk_type->storage_type;
	unifykey_types.ops->read = uk_type->ops->read;
	unifykey_types.ops->write = uk_type->ops->write;

	return 0;
}
EXPORT_SYMBOL(register_unifykey_types);

/**
 *1.init
 * return ok 0, fail 1
 */
static s32 _amlkey_init(u8 *seed, u32 len, int encrypt_type)
{
	s32	ret = 0;
	u32	actual_size = 0;
	struct unifykey_type *uk_type;

	if (storagekey_info.buffer) {
		pr_info("already init!\n");
		goto _out;
	}

	/* get buffer from bl31 */
	storagekey_info.buffer = secure_storage_getbuf(&storagekey_info.size);
	if (!storagekey_info.buffer) {
		pr_err("can't get buffer from bl31!\n");
		ret = -EINVAL;
		goto _out;
	}

	/* fullfill key infos from storage. */
	if (!is_valid_unifykey_storage_type(unifykey_types.storage_type)) {
		pr_err("check whether emmc_key/nand_key driver is enabled\n");
		ret = -EINVAL;
		goto _out;
	}

	uk_type = &unifykey_types;
	if (unlikely(!uk_type->ops->read)) {
		pr_err("the read fun for current unifykey type is NULL\n");
		ret = -EINVAL;
		goto _out;
	}

	ret = uk_type->ops->read(storagekey_info.buffer,
				 storagekey_info.size,
				 &actual_size);
	if (ret) {
		pr_err("fail to read key data\n");
		memset(storagekey_info.buffer, 0, SECURESTORAGE_HEAD_SIZE);
		goto _out;
	}

	if (actual_size >= (1U << 20)) {
		pr_err("really need more than 1M mem? please check\n");
		memset(storagekey_info.buffer, 0, SECURESTORAGE_HEAD_SIZE);
		ret = -EINVAL;
		goto _out;
	}

	storagekey_info.size = min_t(int, actual_size, storagekey_info.size);
	pr_info("buffer=%p, size = %0x!\n", storagekey_info.buffer,
		storagekey_info.size);

_out:
	return ret;
}

int securekey_prebuf_init(void)
{
	securekey_prebuf = kmalloc(SECUREKEY_SIZE, GFP_KERNEL);
	if (!securekey_prebuf)
		return -ENOMEM;

	return 0;
}

void securekey_prebuf_deinit(void)
{
	kfree(securekey_prebuf);
}

static u32 _amlkey_exist(const u8 *name)
{
	unsigned long ret = 0;
	u32	retval;

	if (!name) {
		pr_err("key name is NULL\n");
		return 0;
	}

	ret = secure_storage_query((u8 *)name, &retval);
	if (ret) {
		pr_err("fail to query key %s\n", name);
		retval = 0;
	}

	return retval;
}

static u32 _amlkey_get_attr(const u8 *name)
{
	unsigned long ret = 0;
	u32	retval;

	if (!name) {
		pr_err("key name is NULL\n");
		return 0;
	}

	ret = secure_storage_status((u8 *)name, &retval);
	if (ret) {
		pr_err("fail to obtain status for key %s\n", name);
		retval = 0;
	}

	return retval;
}

static unsigned int _amlkey_size(const u8 *name)
{
	unsigned int	retval;

	if (!name) {
		pr_err("key name is NULL\n");
		retval = 0;
		goto _out;
	}

	if (secure_storage_tell((u8 *)name, &retval)) {
		pr_err("fail to obtain size of key %s\n", name);
		retval = 0;
	}
_out:
	return retval;
}

static unsigned int _amlkey_read(const u8 *name, u8 *buffer, u32 len)
{
	unsigned int retval = 0;

	if (!name) {
		pr_err("key name is NULL\n");
		retval = 0;
		goto _out;
	}
	if (secure_storage_read((u8 *)name, buffer, len, &retval)) {
		pr_err("fail to read key %s\n", name);
		retval = 0;
	}
_out:
	return retval;
}

static ssize_t _amlkey_write(const u8 *name, u8 *buffer,
			     u32 len, u32 attr)
{
	ssize_t retval = 0;
	u32	actual_length;
	u8	*buf = NULL;
	struct unifykey_type *uk_type;

	if (!name) {
		pr_err("key name is NULL\n");
		return retval;
	}

	if (secure_storage_write((u8 *)name, buffer, len, attr)) {
		pr_err("fail to write key %s\n", name);
		retval = 0;
		goto _out;
	}

	retval = (ssize_t)len;

	if (!is_valid_unifykey_storage_type(unifykey_types.storage_type)) {
		pr_err("error: no storage type set\n");
		return 0;
	}
	uk_type = &unifykey_types;

	if (!storagekey_info.buffer) {
		retval = 0;
		goto _out;
	}

	if (!securekey_prebuf)
		return 0;

	if (storagekey_info.size > SECUREKEY_SIZE) {
		pr_err("%s() %d: pre alloc buffer[0x%x] is too small, need size[0x%x].\n",
			__func__, __LINE__, SECUREKEY_SIZE, storagekey_info.size);
		return 0;
	}

	mutex_lock(&securekey_lock);
	memset(securekey_prebuf, 0, SECUREKEY_SIZE);
	buf = securekey_prebuf;

	memcpy(buf, storagekey_info.buffer, storagekey_info.size);
	if (!uk_type->ops->write) {
		pr_err("the write fun for current unifykey type is NULL\n");
		retval = 0;
		mutex_unlock(&securekey_lock);
		goto _out;
	}
	if (uk_type->ops->write(buf, storagekey_info.size, &actual_length)) {
		pr_err("fail to write key data to storage\n");
		retval = 0;
	}

	mutex_unlock(&securekey_lock);

_out:
	return retval;
}

static s32 _amlkey_hash(const u8 *name, u8 *hash)
{
	return secure_storage_verify((u8 *)name, hash);
}

#define DEF_NORMAL_BLOCK_SIZE	(256 * 1024)
static DEFINE_MUTEX(normalkey_lock);
static u32 normal_blksz = DEF_NORMAL_BLOCK_SIZE;
static u32 normal_flashsize = DEF_NORMAL_BLOCK_SIZE;
static u8 *normal_block;

static s32 _amlkey_init_normal(u8 *seed, u32 len, int encrypt_type)
{
	int ret;

	if (!normal_block)
		return -1;

	if (!unifykey_types.ops->read) {
		pr_err("no storage found\n");
		return -1;
	}

	if (normalkey_init())
		return -1;

	mutex_lock(&normalkey_lock);
	ret = unifykey_types.ops->read(normal_block,
				       normal_blksz,
				       &normal_flashsize);
	if (ret) {
		pr_err("read storage fail\n");
		goto finish;
	}

	ret = normalkey_readfromblock(normal_block, normal_flashsize);
	if (ret) {
		pr_err("init block key fail\n");
		goto finish;
	}

	ret = 0;
finish:
	if (ret)
		normalkey_deinit();
	mutex_unlock(&normalkey_lock);

	return ret;
}

static u32 _amlkey_exist_normal(const u8 *name)
{
	struct storage_object *obj;

	mutex_lock(&normalkey_lock);
	obj = normalkey_get(name);
	mutex_unlock(&normalkey_lock);

	return !!obj;
}

static u32 _amlkey_get_attr_normal(const u8 *name)
{
	u32 attr = 0;
	struct storage_object *obj;

	mutex_lock(&normalkey_lock);
	obj = normalkey_get(name);
	if (obj)
		attr = obj->attribute;
	mutex_unlock(&normalkey_lock);

	return attr;
}

static unsigned int _amlkey_size_normal(const u8 *name)
{
	unsigned int size = 0;
	struct storage_object *obj;

	mutex_lock(&normalkey_lock);
	obj = normalkey_get(name);
	if (obj)
		size = obj->datasize;
	mutex_unlock(&normalkey_lock);

	return size;
}

static unsigned int _amlkey_read_normal(const u8 *name, u8 *buffer, u32 len)
{
	unsigned int size = 0;
	struct storage_object *obj;

	mutex_lock(&normalkey_lock);
	obj = normalkey_get(name);
	if (obj && len >= obj->datasize) {
		size = obj->datasize;
		memcpy(buffer, obj->dataptr, size);
	}
	mutex_unlock(&normalkey_lock);

	return size;
}

static ssize_t _amlkey_write_normal(const u8 *name, u8 *buffer,
				    u32 len, u32 attr)
{
	int ret;
	u32 wrtsz = 0;

	if (attr & OBJ_ATTR_SECURE) {
		pr_err("can't write secure key\n");
		return 0;
	}

	if (!unifykey_types.ops->write) {
		pr_err("no storage found\n");
		return 0;
	}

	mutex_lock(&normalkey_lock);
	ret = normalkey_add(name, buffer, len, attr);
	if (ret) {
		pr_err("write key fail\n");
		ret = 0;
		goto unlock;
	}

	ret = normalkey_writetoblock(normal_block, normal_flashsize);
	if (ret) {
		pr_err("write block fail\n");
		ret = 0;
		goto unlock;
	}

	ret = unifykey_types.ops->write(normal_block,
					normal_flashsize,
					&wrtsz);
	if (ret) {
		pr_err("write storage fail\n");
		ret = 0;
		goto unlock;
	}
	ret = len;
unlock:
	mutex_unlock(&normalkey_lock);
	return ret;
}

static s32 _amlkey_hash_normal(const u8 *name, u8 *hash)
{
	int ret = -1;
	struct storage_object *obj;

	mutex_lock(&normalkey_lock);
	obj = normalkey_get(name);
	if (obj) {
		ret = 0;
		memcpy(hash, obj->hashptr, 32);
	}
	mutex_unlock(&normalkey_lock);

	return ret;
}

int normal_key_init(struct platform_device *pdev)
{
	u32 blksz;
	int ret;

	ret = device_property_read_u32(&pdev->dev, "blocksize", &blksz);
	if (!ret && blksz && PAGE_ALIGNED(blksz)) {
		normal_blksz = blksz;
		pr_info("block size from config: %x\n", blksz);
	}

	normal_block = kmalloc(normal_blksz, GFP_KERNEL);
	if (!normal_block)
		return -1;

	return 0;
}

void normal_key_deinit(void)
{
	kfree(normal_block);
}

enum amlkey_if_type {
	IFTYPE_SECURE_STORAGE,
	IFTYPE_NORMAL_STORAGE,
	IFTYPE_MAX
};

struct amlkey_if amlkey_ifs[] = {
	[IFTYPE_SECURE_STORAGE] = {
		.init = _amlkey_init,
		.exist = _amlkey_exist,
		.get_attr = _amlkey_get_attr,
		.size = _amlkey_size,
		.read = _amlkey_read,
		.write = _amlkey_write,
		.hash = _amlkey_hash,
	},
	[IFTYPE_NORMAL_STORAGE] = {
		.init = _amlkey_init_normal,
		.exist = _amlkey_exist_normal,
		.get_attr = _amlkey_get_attr_normal,
		.size = _amlkey_size_normal,
		.read = _amlkey_read_normal,
		.write = _amlkey_write_normal,
		.hash = _amlkey_hash_normal,
	}
};

struct amlkey_if *amlkey_if;

int __init amlkey_if_init(struct platform_device *pdev)
{
	int ret;

	ret = security_key_init(pdev);
	if (ret != -EOPNOTSUPP) {
		amlkey_if = &amlkey_ifs[IFTYPE_SECURE_STORAGE];
		ret = securekey_prebuf_init();
		return ret;
	}

	pr_debug("normal key used!\n");
	ret = normal_key_init(pdev);
	amlkey_if = &amlkey_ifs[IFTYPE_NORMAL_STORAGE];

	return ret;
}

void amlkey_if_deinit(void)
{
	securekey_prebuf_deinit();

	normal_key_deinit();
}
