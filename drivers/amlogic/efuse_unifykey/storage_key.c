// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#ifdef CONFIG_AMLOGIC_EFUSE
#include <linux/amlogic/efuse.h>
#endif
#include <linux/amlogic/key_manage.h>
#include "unifykey.h"
#include "amlkey_if.h"
#include "security_key.h"

#define SECUESTORAGE_HEAD_SIZE		(256)
#define SECUESTORAGE_WHOLE_SIZE		(0x40000)

#undef pr_fmt
#define pr_fmt(fmt) "unifykey: " fmt

struct storagekey_info_t {
	u8  *buffer;
	u32 size;
};

static struct storagekey_info_t storagekey_info = {
	.buffer = NULL,
	/* default size */
	.size = SECUESTORAGE_WHOLE_SIZE,
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
		pr_err("alreay registered\n");
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
s32 amlkey_init(u8 *seed, u32 len, int encrypt_type)
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
		memset(storagekey_info.buffer, 0, SECUESTORAGE_HEAD_SIZE);
		goto _out;
	}

	if (actual_size >= (1U << 20)) {
		pr_err("really need more than 1M mem? please check\n");
		memset(storagekey_info.buffer, 0, SECUESTORAGE_HEAD_SIZE);
		ret = -EINVAL;
		goto _out;
	}

	storagekey_info.size = min_t(int, actual_size, storagekey_info.size);
	pr_info("buffer=%p, size = %0x!\n", storagekey_info.buffer,
		storagekey_info.size);

_out:
	return ret;
}

u32 amlkey_exsit(const u8 *name)
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

u32 amlkey_get_attr(const u8 *name)
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

unsigned int amlkey_size(const u8 *name)
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

unsigned int amlkey_read(const u8 *name, u8 *buffer, u32 len)
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

ssize_t amlkey_write(const u8 *name, u8 *buffer,
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

	buf = kmalloc(storagekey_info.size, GFP_KERNEL);
	if (!buf) {
		retval = 0;
		goto _out;
	}
	memcpy(buf, storagekey_info.buffer, storagekey_info.size);
	if (!uk_type->ops->write) {
		pr_err("the write fun for current unifykey type is NULL\n");
		retval = 0;
		goto _out;
	}
	if (uk_type->ops->write(buf, storagekey_info.size, &actual_length)) {
		pr_err("fail to write key data to storage\n");
		retval = 0;
	}

_out:
	kfree(buf);
	return retval;
}

s32 amlkey_hash(const u8 *name, u8 *hash)
{
	return secure_storage_verify((u8 *)name, hash);
}
