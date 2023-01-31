// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/mtd/mtd.h>
#include <linux/amlogic/aml_rsv.h>
#include <linux/amlogic/key_manage.h>

static struct meson_rsv_info_t *meson_rsv_key;

/*
 * This function reads the u-boot keys.
 */
s32 amlnf_key_read(u8 *buf, u32 len, u32 *actual_length)
{
	struct meson_rsv_info_t *aml_key = meson_rsv_key;
	u32 keysize = 0;

	if (!meson_rsv_key) {
		pr_info("%s(): amlnf key not ready yet!",
			__func__);
		return -EFAULT;
	}

	keysize = aml_key->size;
	*actual_length = keysize;

	if (len > keysize) {
		/*
		 *No return here! keep consistent, should memset zero
		 *for the rest.
		 */
		pr_info("%s read key len exceed,len 0x%x,keysize 0x%x\n",
			__func__, len, keysize);
		memset(buf + keysize, 0, len - keysize);
	}

	meson_rsv_key_read(buf, min_t(int, keysize, len));

	return 0;
}

/*
 * This function write the keys.
 */
s32 amlnf_key_write(u8 *buf, u32 len, u32 *actual_length)
{
	struct meson_rsv_info_t *aml_key = meson_rsv_key;
	/*struct mtd_info *mtd = aml_chip->mtd;*/
	u32 keysize = 0;
	int error = 0;

	if (!meson_rsv_key) {
		pr_info("%s(): amlnf key not ready yet!",
			__func__);
		return -EFAULT;
	}

	keysize = aml_key->size;
	*actual_length = keysize;

	if (len > keysize) {
		/*
		 *No return here! keep consistent, should memset zero
		 *for the rest.
		 */
		pr_info("%s write key len exceed,len 0x%x,keysize 0x%x\n",
			__func__, len, keysize);
		memset(buf + keysize, 0, len - keysize);
	}

	error = meson_rsv_key_write(buf, len);

	return error;
}

int amlnf_key_erase(void)
{
	int ret = 0;

	if (!meson_rsv_key) {
		pr_info("%s amlnf not ready yet!\n", __func__);
		return -1;
	}

	return ret;
}

int meson_rsv_register_unifykey(struct meson_rsv_info_t *key)
{
	int ret = 0;
	struct unifykey_type *uk_type;

	uk_type = kzalloc(sizeof(*uk_type), GFP_KERNEL);
	if (!uk_type)
		return -ENOMEM;

	uk_type->ops = kzalloc(sizeof(*uk_type->ops), GFP_KERNEL);
	if (!uk_type->ops) {
		ret = -ENOMEM;
		goto exit;
	}

	/* avoid null */
	meson_rsv_key = key;

	/**need test**/
	uk_type->storage_type = UNIFYKEY_STORAGE_TYPE_NAND;
	/*
	 * pr_info("%s key func read: 0x%px, write: 0x%px\n",
	 *	__func__, amlnf_key_read, amlnf_key_write);
	 */
	uk_type->ops->read = amlnf_key_read;
	uk_type->ops->write = amlnf_key_write;
	/*
	 *pr_info("%s key func read: 0x%px, write: 0x%px\n",
	 *__func__, uk_type->ops->read, uk_type->ops->write);
	 */
	ret = register_unifykey_types(uk_type);

	kfree(uk_type->ops);
exit:
	kfree(uk_type);
	return ret;
}

