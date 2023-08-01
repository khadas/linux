// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/aml_kt.h>

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/amlogic/iomap.h>
#include "aml_kt_log.h"
#include "aml_kt_dev.h"

#define AML_KT_DEVICE_NAME "aml_kt"
#define DEVICE_INSTANCES 1

#define KT_MODE_READ  (0)
#define KT_MODE_NAGRA (1)
#define KT_MODE_NSK   (2)
#define KT_MODE_HOST  (3)

#define KT_CLEAN_KTE  (1)
#define KT_STS_OK     (0)
#define KT_PENDING    (1)
#define KT_STS_DENIED (3)
#define KT_SUCCESS    (0)
#define KT_ERROR      (-1)

#define KT_IV_SPEC_ALGO	  (0xff)

#define KT_PENDING_WAIT_TIMEOUT (20000)

#define KTE_PENDING_OFFSET  (31)
#define KTE_PENDING_MASK    (1)
#define KTE_STATUS_OFFSET   (29)
#define KTE_STATUS_MASK	    (3)
#define KTE_CLEAN_OFFSET    (28)
#define KTE_CLEAN_MASK	    (1)
#define KTE_MODE_OFFSET	    (26)
#define KTE_MODE_MASK	    (3)
#define KTE_FLAG_OFFSET	    (24)
#define KTE_FLAG_MASK	    (3)
#define KTE_KEYALGO_OFFSET  (20)
#define KTE_KEYALGO_MASK    (0xf)
#define KTE_USERID_OFFSET   (16)
#define KTE_USERID_MASK	    (0xf)
#define KTE_KTE_OFFSET	    (8)
#define KTE_KTE_MASK	    (0xff)

#define HANDLE_TO_ENTRY(h) ((h) & ~(1 << KT_IV_FLAG_OFFSET))

enum KT_ALGO_CAPABILITY {
	KT_CAP_AES = 0x1,
	KT_CAP_DES = 0x2,
	KT_CAP_TDES = 0x4,
	KT_CAP_S17 = 0x8,
	KT_CAP_SM4 = 0x10,
	KT_CAP_NDL = 0x80,
	KT_CAP_ND = 0x100,
	KT_CAP_CSA3 = 0x200,
	KT_CAP_CSA2 = 0x400,
	KT_CAP_HMAC = 0x2000,
};

enum KT_UID_CAPABILITY {
	KT_CAP_M2M_0 = 0x1,
	KT_CAP_M2M_1 = 0x2,
	KT_CAP_M2M_2 = 0x4,
	KT_CAP_M2M_3 = 0x8,
	KT_CAP_M2M_4 = 0x10,
	KT_CAP_M2M_5 = 0x20,
	KT_CAP_M2M_ANY = 0x80,
	KT_CAP_TSD = 0x100,
	KT_CAP_TSN = 0x200,
	KT_CAP_TSE = 0x400,
};

/*bit 0~11: frobenious cycles*/
/*bit 12: variant cryptography
 *        0: variant 5
 *        1: variant 6
 */
 /*variant: 6, frobenious cycles:0x5a5*/
#define S17_CFG_DEFAULT     (0x15a5)

static dev_t aml_kt_devt;
struct aml_kt_dev aml_kt_dev;
EXPORT_SYMBOL(aml_kt_dev);

static struct class *aml_kt_class;
static struct dentry *aml_kt_debug_dent;
u32 kt_log_level = 3;

static int aml_kt_init_dbgfs(void)
{
	if (!aml_kt_debug_dent) {
		aml_kt_debug_dent = debugfs_create_dir("aml_kt", NULL);
		if (!aml_kt_debug_dent) {
			LOGE("can not create debugfs directory\n");
			return -ENOMEM;
		}

		debugfs_create_u32("log_level", 0644, aml_kt_debug_dent, &kt_log_level);
	}
	return 0;
}

static int aml_kt_lock(struct aml_kt_dev *dev)
{
	int cnt = 0;
	int ret = KT_SUCCESS;

	while (ioread32((char *)dev->base_addr + dev->reg.rdy_offset) ==
	       KT_STS_OK) {
		if (cnt++ > KT_PENDING_WAIT_TIMEOUT) {
			LOGE("Error: wait KT ready timeout\n");
			ret = KT_ERROR;
		}
	}

	return ret;
}

static void aml_kt_unlock(struct aml_kt_dev *dev)
{
	iowrite32(1, (char *)dev->base_addr + dev->reg.rdy_offset);
}

static bool aml_kt_handle_valid(struct aml_kt_dev *dev, u32 handle)
{
	u32 entry = HANDLE_TO_ENTRY(handle);
	u8 is_iv = handle >> KT_IV_FLAG_OFFSET;

	if (is_iv) {
		if (entry < dev->ive_start || dev->ive_end <= entry) {
			LOGE("Error: Invalid handle (%#x)\n", handle);
			return false;
		}
	} else {
		if (entry < dev->kte_start || dev->kte_end <= entry) {
			LOGE("Error: Invalid handle (%#x)\n", handle);
			return false;
		}
	}

	return true;
}

int aml_kt_handle_to_kte(struct aml_kt_dev *dev, u32 handle, u32 *kte)
{
	if (unlikely(!dev)) {
		LOGE("Empty aml_kt_dev\n");
		return KT_ERROR;
	}

	if (!aml_kt_handle_valid(dev, handle)) {
		LOGE("Invalid handle\n");
		return KT_ERROR;
	}

	if (!kte) {
		LOGE("Invalid kte\n");
		return KT_ERROR;
	}

	*kte = HANDLE_TO_ENTRY(handle);

	return KT_SUCCESS;
}
EXPORT_SYMBOL(aml_kt_handle_to_kte);

static int aml_kt_read_pending(struct aml_kt_dev *dev)
{
	int ret = KT_ERROR;
	int cnt = 0;
	u32 reg_ret = 0;

	do {
		reg_ret = ioread32((char *)dev->base_addr + dev->reg.cfg_offset);
		if (cnt++ > KT_PENDING_WAIT_TIMEOUT) {
			LOGE("Error: wait KT pending done timeout\n");
			ret = KT_ERROR;
			return ret;
		}
	} while (reg_ret & (KT_PENDING << KTE_PENDING_OFFSET));

	reg_ret = (reg_ret >> KTE_STATUS_OFFSET) & KTE_STATUS_MASK;
	if (reg_ret == KT_STS_OK) {
		ret = KT_SUCCESS;
	} else {
		LOGE("Error: KT return error:%#x\n", reg_ret);
		ret = KT_ERROR;
	}

	return ret;
}

/*
 * Determine whether the slot is reserved (chips: A5/C3)
 */
static bool aml_kt_slot_reserved(u32 index)
{
	u32 i;
	bool reserved = false;
	u32 res_size = 0;
	u32 res_list[] = {5, 6, 7, 8, 9, 10, 11, 12};

	res_size = ARRAY_SIZE(res_list);
	for (i = 0; i < res_size; i++) {
		if (index == res_list[i]) {
			reserved = true;
			break;
		}
	}

	return reserved;
}

int aml_kt_get_status(struct aml_kt_dev *dev, u32 handle, u32 *key_sts)
{
	int ret = KT_SUCCESS;
	u32 kte = 0;
	u32 reg_val = 0;
	u32 reg_offset = 0;
	u32 reg_ret = 0;

	if (unlikely(!dev)) {
		LOGE("Empty aml_kt_dev\n");
		return KT_ERROR;
	}

	ret = aml_kt_handle_to_kte(dev, handle, &kte);
	if (ret != KT_SUCCESS) {
		LOGE("kte get error\n");
		return KT_ERROR;
	}

	mutex_lock(&dev->lock);
	if (aml_kt_lock(dev) != KT_SUCCESS) {
		LOGE("lock error\n");
		ret = KT_ERROR;
		goto unlock_mutex;
	}

	reg_offset = dev->reg.cfg_offset;
	reg_val = (KT_PENDING << KTE_PENDING_OFFSET |
			KT_MODE_READ << KTE_MODE_OFFSET |
			kte << KTE_KTE_OFFSET);
	iowrite32(reg_val, (char *)dev->base_addr + reg_offset);

	if (aml_kt_read_pending(dev) != KT_SUCCESS) {
		LOGE("pending error kte[%d]\n", kte);
		ret = KT_ERROR;
		goto unlock_kt;
	}

	reg_ret = ioread32((char *)dev->base_addr + dev->reg.sts_offset);
	if (reg_ret != KT_ERROR) {
		LOGD("kte=%d KT_REE_STS=0x%08x\n", kte, reg_ret);
		memcpy(key_sts, &reg_ret, sizeof(reg_ret));
		ret = KT_SUCCESS;
		goto unlock_kt;
	}

unlock_kt:
	aml_kt_unlock(dev);
unlock_mutex:
	mutex_unlock(&dev->lock);

	return ret;
}
EXPORT_SYMBOL(aml_kt_get_status);

int aml_kt_alloc(struct aml_kt_dev *dev, u32 flag, u32 *handle)
{
	int res = KT_SUCCESS;
	u32 entry = 0;
	u8 is_iv = 0;
	u32 entry_start = 0;
	u32 entry_end = 0;

	if (unlikely(!dev)) {
		LOGE("Empty aml_kt_dev\n");
		return KT_ERROR;
	}

	if (!handle) {
		LOGE("handle is null\n");
		return KT_ERROR;
	}

	if (flag == AML_KT_ALLOC_FLAG_IV) {
		is_iv = 1;
		entry_start = dev->ive_start;
		entry_end = dev->ive_end;
	} else if (flag == AML_KT_ALLOC_FLAG_HOST) {
		entry_start = dev->kte_start;
		entry_end = dev->kte_end;
	} else if (flag == AML_KT_ALLOC_FLAG_NSK_M2M) {
		/* TO-DO */
	} else {
		entry_start = dev->kte_start;
		entry_end = dev->kte_end;
	}

	mutex_lock(&dev->lock);
	for (entry = entry_start; entry < entry_end; entry++) {
		if (is_iv) {
			if (dev->kt_iv_slot[entry])
				continue;

			dev->kt_iv_slot[entry] = kzalloc(sizeof(*dev->kt_iv_slot[entry]),
				GFP_KERNEL);
			if (!dev->kt_iv_slot[entry]) {
				LOGE("Error: KT ive kzalloc failed\n");
				res = KT_ERROR;
				goto exit;
			}
		} else {
			if (dev->kt_slot[entry])
				continue;

			/* Skip reserved key slots */
			if (dev->kt_reserved && aml_kt_slot_reserved(entry))
				continue;

			dev->kt_slot[entry] = kzalloc(sizeof(*dev->kt_slot[entry]),
				GFP_KERNEL);
			if (!dev->kt_slot[entry]) {
				LOGE("Error: KT kte kzalloc failed\n");
				res = KT_ERROR;
				goto exit;
			}
		}
		break;
	}

	if (entry < entry_end) {
		*handle = entry | (is_iv << KT_IV_FLAG_OFFSET);
		LOGI("flag:%#x, is_iv:%d, handle:%#x\n", flag, is_iv, *handle);
		res = KT_SUCCESS;
	} else {
		LOGE("Error: KT alloc return error, no kte/ive available\n");
		res = KT_ERROR;
	}

exit:
	mutex_unlock(&dev->lock);

	return res;
}
EXPORT_SYMBOL(aml_kt_alloc);

int aml_kt_config(struct aml_kt_dev *dev, struct amlkt_cfg_param key_cfg)
{
	int ret = KT_SUCCESS;
	u32 index = 0;
	u8 is_iv = 0;
	u8 is_dsc = 0;

	if (unlikely(!dev)) {
		LOGE("Empty aml_kt_dev\n");
		return KT_ERROR;
	}

	ret = aml_kt_handle_to_kte(dev, key_cfg.handle, &index);
	if (ret != KT_SUCCESS) {
		LOGE("index get error\n");
		return ret;
	}

	is_iv = key_cfg.handle >> KT_IV_FLAG_OFFSET;
	if (is_iv) {
		if (!dev->kt_iv_slot[index]) {
			LOGE("kt_iv_slot is null\n");
			return KT_ERROR;
		}
	} else {
		if (!dev->kt_slot[index]) {
			LOGE("kt_slot is null\n");
			return KT_ERROR;
		}
	}

	LOGD("--------------------------------------------------------------\n");
	LOGD("flag:%d, algo:%d, uid:%d, src:%d\n", key_cfg.key_flag, key_cfg.key_algo,
	     key_cfg.key_userid, key_cfg.key_source);

	/* Conversion T5W key algorithm */
	if (dev->algo_cap == 0x407 && dev->user_cap == 0x600) {
		if (key_cfg.key_algo == AML_KT_ALGO_AES) {
			key_cfg.key_algo = 2;
			LOGD("AES conversion\n");
		} else if (key_cfg.key_algo == AML_KT_ALGO_DES) {
			key_cfg.key_algo = 0;
			LOGD("DES conversion\n");
		} else if (key_cfg.key_algo == AML_KT_ALGO_CSA2) {
			key_cfg.key_algo = 0;
			LOGD("CSA2 conversion\n");
		} else {
			LOGD("No need conversion\n");
		}
	}

	/* Check Invalid key algorithm */
	LOGD("dev->algo_cap:0x%x\n", dev->algo_cap);
	LOGD("dev->user_cap:0x%x\n", dev->user_cap);
	switch (key_cfg.key_algo) {
	case AML_KT_ALGO_AES:
		if (!(dev->algo_cap & KT_CAP_AES))
			goto error_algo;
		break;
	case AML_KT_ALGO_TDES:
		if (!(dev->algo_cap & KT_CAP_TDES))
			goto error_algo;
		break;
	case AML_KT_ALGO_DES:
		if (!(dev->algo_cap & KT_CAP_DES))
			goto error_algo;
		break;
	case AML_KT_ALGO_S17:
		if (!(dev->algo_cap & KT_CAP_S17))
			goto error_algo;
		break;
	case AML_KT_ALGO_SM4:
		if (!(dev->algo_cap & KT_CAP_SM4))
			goto error_algo;
		break;
	case AML_KT_ALGO_NDL:
		if (!(dev->algo_cap & KT_CAP_NDL))
			goto error_algo;
		break;
	case AML_KT_ALGO_ND:
		if (!(dev->algo_cap & KT_CAP_ND))
			goto error_algo;
		break;
	case AML_KT_ALGO_CSA3:
		if (!(dev->algo_cap & KT_CAP_CSA3))
			goto error_algo;
		break;
	case AML_KT_ALGO_CSA2:
		if (!(dev->algo_cap & KT_CAP_CSA2))
			goto error_algo;
		break;
	case AML_KT_ALGO_HMAC:
		if (!(dev->algo_cap & KT_CAP_HMAC))
			goto error_algo;
		break;
	default:
		goto error_algo;
	}

	/* Check Invalid key user id */
	switch (key_cfg.key_userid) {
	case AML_KT_USER_M2M_0:
		if (!(dev->user_cap & KT_CAP_M2M_0))
			goto error_user;
		break;
	case AML_KT_USER_M2M_1:
		if (!(dev->user_cap & KT_CAP_M2M_1))
			goto error_user;
		break;
	case AML_KT_USER_M2M_2:
		if (!(dev->user_cap & KT_CAP_M2M_2))
			goto error_user;
		break;
	case AML_KT_USER_M2M_3:
		if (!(dev->user_cap & KT_CAP_M2M_3))
			goto error_user;
		break;
	case AML_KT_USER_M2M_4:
		if (!(dev->user_cap & KT_CAP_M2M_4))
			goto error_user;
		break;
	case AML_KT_USER_M2M_5:
		if (!(dev->user_cap & KT_CAP_M2M_5))
			goto error_user;
		break;
	case AML_KT_USER_M2M_ANY:
		if (!(dev->user_cap & KT_CAP_M2M_ANY))
			goto error_user;
		break;
	case AML_KT_USER_TSD:
		is_dsc = 1;
		if (!(dev->user_cap & KT_CAP_TSD))
			goto error_user;
		break;
	case AML_KT_USER_TSN:
		is_dsc = 1;
		if (!(dev->user_cap & KT_CAP_TSN))
			goto error_user;
		break;
	case AML_KT_USER_TSE:
		is_dsc = 1;
		if (!(dev->user_cap & KT_CAP_TSE))
			goto error_user;
		break;
	default:
		goto error_user;
	}

	/* Special case for S17 key algorithm.
	 * It is used only for descrambler.
	 */
	if (key_cfg.key_algo == AML_KT_ALGO_S17 && is_dsc) {
		if (key_cfg.ext_value == 0) {
			iowrite32(S17_CFG_DEFAULT,
				(char *)dev->base_addr + dev->reg.s17_cfg_offset);
			LOGD("default frobenius :0x%0x\n", S17_CFG_DEFAULT);
		} else {
			iowrite32(key_cfg.ext_value,
				(char *)dev->base_addr + dev->reg.s17_cfg_offset);
			LOGD("frobenius :0x%0x\n", key_cfg.ext_value);
		}
	}

	if (is_iv)
		memcpy(dev->kt_iv_slot[index], &key_cfg, sizeof(struct amlkt_cfg_param));
	else
		memcpy(dev->kt_slot[index], &key_cfg, sizeof(struct amlkt_cfg_param));
	return ret;

error_algo:
	LOGE("invalid key algorithm:%d\n", key_cfg.key_algo);
	goto exit;

error_user:
	LOGE("invalid key user:%d\n", key_cfg.key_userid);
	goto exit;

exit:
	return KT_ERROR;
}
EXPORT_SYMBOL(aml_kt_config);

static int aml_kt_write_cfg(struct aml_kt_dev *dev,
	u32 handle, u32 key_mode, struct amlkt_cfg_param *key_cfg, u32 func_id)
{
	int ret = KT_SUCCESS;
	u32 kte = 0;
	u32 reg_val = 0;
	u32 reg_offset = 0;
	u8 is_iv = 0;

	ret = aml_kt_handle_to_kte(dev, handle, &kte);
	if (ret != KT_SUCCESS) {
		LOGE("kte get error\n");
		return KT_ERROR;
	}

	is_iv = handle >> KT_IV_FLAG_OFFSET;
	if (key_cfg->key_source == AML_KT_SRC_NSK) {
		// Divide with 3 for getting NSK KTE group ID
		kte = kte / 3;
	}

	reg_offset = dev->reg.cfg_offset;
	reg_val = (KT_PENDING << KTE_PENDING_OFFSET) | (key_mode << KTE_MODE_OFFSET)
			| (key_cfg->key_flag << KTE_FLAG_OFFSET)
			| ((is_iv ? KT_IV_SPEC_ALGO : key_cfg->key_algo) << KTE_KEYALGO_OFFSET)
			| (key_cfg->key_userid << KTE_USERID_OFFSET) | (kte << KTE_KTE_OFFSET)
			| func_id;
	iowrite32(reg_val, (char *)dev->base_addr + reg_offset);

	if (aml_kt_read_pending(dev) != KT_SUCCESS) {
		LOGE("Pending error\n");
		return KT_ERROR;
	}

	return KT_SUCCESS;
}

static void aml_kt_write_key(struct aml_kt_dev *dev, u32 offset, const u8 *key, u32 keylen)
{
	u32 tmp_key = 0;

	memcpy((void *)&tmp_key, key, keylen);
	iowrite32(tmp_key, (char *)dev->base_addr + offset);
}

static int aml_kt_set_inter_key(struct aml_kt_dev *dev, u32 handle, struct amlkt_cfg_param *key_cfg,
				u8 *key, u32 keylen, u32 key_mode)
{
	int ret = KT_SUCCESS;
	u32 func_id = 0;

	if (!key_cfg) {
		LOGE("key_cfg is null\n");
		return KT_ERROR;
	}

	if (!key || keylen == 0) {
		LOGE("key or keylen is null\n");
		return KT_ERROR;
	}

	mutex_lock(&dev->lock);
	if (aml_kt_lock(dev) != KT_SUCCESS) {
		LOGE("lock error\n");
		ret = KT_ERROR;
		goto unlock_mutex;
	}

	LOGD("--------------------------------------------------------------\n");
	LOGD("flag:%d, algo:%d, uid:%d, src:%d\n", key_cfg->key_flag, key_cfg->key_algo,
	     key_cfg->key_userid, key_cfg->key_source);

	if (keylen > 16 && keylen <= 32) {
		if (keylen >= 4)
			aml_kt_write_key(dev, dev->reg.key0_offset, key, 4);

		if (keylen >= 8)
			aml_kt_write_key(dev, dev->reg.key1_offset, key + 4, 4);

		if (keylen >= 12)
			aml_kt_write_key(dev, dev->reg.key2_offset, key + 8, 4);

		if (keylen >= 16)
			aml_kt_write_key(dev, dev->reg.key3_offset, key + 12, 4);

		func_id = 6;
		ret = aml_kt_write_cfg(dev, handle, key_mode, key_cfg, func_id);
		if (ret == KT_ERROR) {
			LOGE("aml_kt_write_cfg error\n");
			goto unlock_kt;
		}

		if (keylen >= 20)
			aml_kt_write_key(dev, dev->reg.key0_offset, key + 16, 4);

		if (keylen >= 24)
			aml_kt_write_key(dev, dev->reg.key1_offset, key + 20, 4);

		if (keylen >= 28)
			aml_kt_write_key(dev, dev->reg.key2_offset, key + 24, 4);

		if (keylen >= 32)
			aml_kt_write_key(dev, dev->reg.key3_offset, key + 28, 4);

		func_id = 7;
		ret = aml_kt_write_cfg(dev, handle, key_mode, key_cfg, func_id);
		if (ret == KT_ERROR) {
			LOGE("aml_kt_write_cfg error\n");
			goto unlock_kt;
		}
	} else if (keylen <= 16) {
		if (keylen >= 4)
			aml_kt_write_key(dev, dev->reg.key0_offset, key, 4);

		if (keylen >= 8)
			aml_kt_write_key(dev, dev->reg.key1_offset, key + 4, 4);

		if (keylen >= 12)
			aml_kt_write_key(dev, dev->reg.key2_offset, key + 8, 4);

		if (keylen >= 16)
			aml_kt_write_key(dev, dev->reg.key3_offset, key + 12, 4);

		ret = aml_kt_write_cfg(dev, handle, key_mode, key_cfg, func_id);
		if (ret == KT_ERROR) {
			LOGE("aml_kt_write_cfg error\n");
			goto unlock_kt;
		}
	} else {
		LOGE("Error: unsupported keylen:%d\n", keylen);
		ret = KT_ERROR;
		goto unlock_kt;
	}

unlock_kt:
	aml_kt_unlock(dev);
unlock_mutex:
	mutex_unlock(&dev->lock);

	return ret;
}

static int aml_kt_set_inter_hw_key(struct aml_kt_dev *dev, u32 handle,
				   struct amlkt_cfg_param *key_cfg, u32 key_mode)
{
	int ret = KT_SUCCESS;
	u32 func_id = 0;

	if (!key_cfg) {
		LOGE("key_cfg is null\n");
		return KT_ERROR;
	}

	mutex_lock(&dev->lock);
	if (aml_kt_lock(dev) != KT_SUCCESS) {
		LOGE("lock error\n");
		ret = KT_ERROR;
		goto unlock_mutex;
	}

	LOGD("--------------------------------------------------------------\n");
	LOGD("flag:%d, algo:%d, uid:%d, src:%d\n", key_cfg->key_flag, key_cfg->key_algo,
	     key_cfg->key_userid, key_cfg->key_source);

	ret = aml_kt_write_cfg(dev, handle, key_mode, key_cfg, func_id);
	if (ret == KT_ERROR) {
		LOGE("aml_kt_write_cfg error\n");
		goto unlock_kt;
	}

unlock_kt:
	aml_kt_unlock(dev);
unlock_mutex:
	mutex_unlock(&dev->lock);

	return ret;
}

int aml_kt_set_host_key(struct aml_kt_dev *dev, struct amlkt_set_key_param *key_param)
{
	int ret = KT_SUCCESS;
	u32 index = 0;
	u32 key_source = 0;
	u8 is_iv = 0;
	u32 key_sts = 0;

	ret = aml_kt_handle_to_kte(dev, key_param->handle, &index);
	if (ret != KT_SUCCESS) {
		LOGE("index get error\n");
		return ret;
	}

	is_iv = key_param->handle >> KT_IV_FLAG_OFFSET;
	if (is_iv) {
		if (!dev->kt_iv_slot[index])
			return KT_ERROR;
		key_source = dev->kt_iv_slot[index]->key_source;
	} else {
		if (!dev->kt_slot[index])
			return KT_ERROR;
		key_source = dev->kt_slot[index]->key_source;
	}

	switch (key_source) {
	case AML_KT_SRC_REE_HOST:
		if (is_iv) {
			ret = aml_kt_set_inter_key(dev, key_param->handle,
							dev->kt_iv_slot[index], key_param->key,
							key_param->key_len, KT_MODE_HOST);
		} else {
			ret = aml_kt_set_inter_key(dev, key_param->handle,
						dev->kt_slot[index], key_param->key,
						key_param->key_len, KT_MODE_HOST);
		}
		break;
	default:
		LOGE("Not support key_source[%d]\n", key_source);
		ret = KT_ERROR;
		return ret;
	}

	if (ret == KT_SUCCESS && kt_log_level <= LOG_DEBUG)
		aml_kt_get_status(dev, key_param->handle, &key_sts);

	return ret;
}
EXPORT_SYMBOL(aml_kt_set_host_key);

int aml_kt_set_hw_key(struct aml_kt_dev *dev, u32 handle)
{
	int ret = KT_SUCCESS;
	u32 index = 0;
	u32 key_source = 0;
	u32 key_sts = 0;
	u8 is_iv = 0;

	ret = aml_kt_handle_to_kte(dev, handle, &index);
	if (ret != KT_SUCCESS) {
		LOGE("index get error\n");
		return KT_ERROR;
	}

	is_iv = handle >> KT_IV_FLAG_OFFSET;
	if (is_iv) {
		if (!dev->kt_iv_slot[index])
			return KT_ERROR;
		key_source = dev->kt_iv_slot[index]->key_source;
	} else {
		if (!dev->kt_slot[index])
			return KT_ERROR;
		key_source = dev->kt_slot[index]->key_source;
	}

	switch (key_source) {
	case AML_KT_SRC_REE_CERT:
	case AML_KT_SRC_REEKL_NAGRA:
		if (is_iv) {
			ret = aml_kt_set_inter_hw_key(dev, handle, dev->kt_iv_slot[index],
								KT_MODE_NAGRA);
		} else {
			ret = aml_kt_set_inter_hw_key(dev, handle, dev->kt_slot[index],
							KT_MODE_NAGRA);
		}
		break;
	case AML_KT_SRC_NSK:
		if (is_iv) {
			ret = aml_kt_set_inter_hw_key(dev, handle, dev->kt_iv_slot[index],
								KT_MODE_NSK);
		} else {
			ret = aml_kt_set_inter_hw_key(dev, handle, dev->kt_slot[index],
								KT_MODE_NSK);
		}
		break;
	default:
		LOGE("Not support key_source[%d]\n", key_source);
		ret = KT_ERROR;
		break;
	}

	if (ret == KT_SUCCESS && kt_log_level <= LOG_DEBUG)
		aml_kt_get_status(dev, handle, &key_sts);

	return ret;
}
EXPORT_SYMBOL(aml_kt_set_hw_key);

static int aml_kt_invalidate(struct aml_kt_dev *dev, u32 handle)
{
	int ret = KT_SUCCESS;
	u32 kte = 0;
	u32 reg_val = 0;
	u32 reg_offset = 0;

	if (unlikely(!dev)) {
		LOGE("Empty aml_kt_dev\n");
		return KT_ERROR;
	}

	ret = aml_kt_handle_to_kte(dev, handle, &kte);
	if (ret != KT_SUCCESS) {
		LOGE("kte get error\n");
		return ret;
	}

	mutex_lock(&dev->lock);
	if (aml_kt_lock(dev) != KT_SUCCESS) {
		ret = KT_ERROR;
		goto unlock_mutex;
	}

	reg_offset = dev->reg.cfg_offset;
	reg_val = (KT_PENDING << KTE_PENDING_OFFSET |
			KT_CLEAN_KTE << KTE_CLEAN_OFFSET |
			KT_MODE_HOST << KTE_MODE_OFFSET |
			kte << KTE_KTE_OFFSET);
	iowrite32(reg_val, (char *)dev->base_addr + reg_offset);

	if (aml_kt_read_pending(dev) != KT_SUCCESS) {
		LOGE("pending error\n");
		ret = KT_ERROR;
		goto unlock_kt;
	} else {
		ret = KT_SUCCESS;
	}

unlock_kt:
	aml_kt_unlock(dev);
unlock_mutex:
	mutex_unlock(&dev->lock);

	return ret;
}

int aml_kt_free(struct aml_kt_dev *dev, u32 handle)
{
	int ret = KT_SUCCESS;
	u32 kte = 0;
	u8 is_iv = 0;

	if (unlikely(!dev)) {
		LOGE("Empty aml_kt_dev\n");
		return KT_ERROR;
	}

	ret = aml_kt_handle_to_kte(dev, handle, &kte);
	if (ret != KT_SUCCESS) {
		LOGE("kte get error\n");
		return ret;
	}

	is_iv = handle >> KT_IV_FLAG_OFFSET;
	ret = aml_kt_invalidate(dev, handle);

	mutex_lock(&dev->lock);
	if (is_iv) {
		kfree(dev->kt_iv_slot[kte]);
		dev->kt_iv_slot[kte] = NULL;
	} else {
		kfree(dev->kt_slot[kte]);
		dev->kt_slot[kte] = NULL;
	}
	mutex_unlock(&dev->lock);

	if (ret != KT_SUCCESS) {
		LOGE("Error: KT invalidate clean kte error\n");
		ret = KT_ERROR;
	} else {
		ret = KT_SUCCESS;
	}

	return ret;
}
EXPORT_SYMBOL(aml_kt_free);

int aml_kt_open(struct inode *inode, struct file *filp)
{
	struct aml_kt_dev *dev;

	dev = container_of(inode->i_cdev, struct aml_kt_dev, cdev);
	filp->private_data = dev;

	return 0;
}

int aml_kt_release(struct inode *inode, struct file *filp)
{
	if (!filp->private_data)
		return 0;

	filp->private_data = NULL;

	return 0;
}

static long aml_kt_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct aml_kt_dev *dev = filp->private_data;
	struct amlkt_alloc_param alloc_param;
	struct amlkt_cfg_param cfg_param;
	struct amlkt_set_key_param key_param;
	u32 handle = 0;
	int ret = 0;

	if (unlikely(!dev)) {
		LOGE("Empty aml_kt_dev\n");
		return -EINVAL;
	}

	if (!dev->base_addr) {
		LOGE("ERROR: iobase is zero\n");
		return -EFAULT;
	}

	switch (cmd) {
	case AML_KT_ALLOC:
		memset(&alloc_param, 0, sizeof(alloc_param));
		if (copy_from_user(&alloc_param, (void __user *)arg, sizeof(alloc_param))) {
			LOGE("aml_kt_alloc copy_from_user error\n");
			return -EFAULT;
		}

		ret = aml_kt_alloc(dev, alloc_param.flag, &alloc_param.handle);
		if (ret != 0) {
			LOGE("aml_kt_alloc failed retval=0x%08x\n", ret);
			return -EFAULT;
		}

		ret = copy_to_user((void __user *)arg, &alloc_param, sizeof(alloc_param));
		if (unlikely(ret)) {
			LOGE("aml_kt_alloc copy_to_user error\n");
			return -EFAULT;
		}
		break;
	case AML_KT_CONFIG:
		memset(&cfg_param, 0, sizeof(cfg_param));
		if (copy_from_user(&cfg_param, (void __user *)arg, sizeof(cfg_param))) {
			LOGE("aml_kt_config copy_from_user error\n");
			return -EFAULT;
		}

		ret = aml_kt_config(dev, cfg_param);
		if (ret != 0) {
			LOGE("aml_kt_config failed retval=0x%08x\n", ret);
			return -EFAULT;
		}
		break;
	case AML_KT_SET:
		memset(&key_param, 0, sizeof(key_param));
		if (copy_from_user(&key_param, (void __user *)arg, sizeof(key_param))) {
			LOGE("aml_kt_set_host_key copy_from_user error\n");
			return -EFAULT;
		}

		ret = aml_kt_set_host_key(dev, &key_param);
		if (ret != 0) {
			LOGE("aml_kt_set failed retval=0x%08x\n", ret);
			return -EFAULT;
		}
		break;
	case AML_KT_HW_SET:
		ret = get_user(handle, (u32 __user *)arg);
		if (unlikely(ret)) {
			LOGE("aml_kt_set_hw_key get_user error\n");
			return ret;
		}

		ret = aml_kt_set_hw_key(dev, handle);
		if (ret != 0) {
			LOGE("aml_kt_hw_set failed retval=0x%08x\n", ret);
			return -EFAULT;
		}
		break;
	case AML_KT_FREE:
		ret = get_user(handle, (u32 __user *)arg);
		if (unlikely(ret)) {
			LOGE("aml_kt_free get_user error\n");
			return ret;
		}

		ret = aml_kt_free(dev, handle);
		if (ret != 0) {
			LOGE("aml_kt_free failed retval=0x%08x\n", ret);
			return -EFAULT;
		}
		break;
	case AML_KT_INVALIDATE:
		ret = get_user(handle, (u32 __user *)arg);
		if (unlikely(ret)) {
			LOGE("aml_kt_invalidate get_user error\n");
			return ret;
		}

		ret = aml_kt_invalidate(dev, handle);
		if (ret != 0) {
			LOGE("aml_kt_invalidate failed retval=0x%08x\n", ret);
			return -EFAULT;
		}
		break;
	default:
		LOGE("Unknown cmd: %d\n", cmd);
		return -EFAULT;
	}

	return 0;
}

static const struct file_operations aml_kt_fops = {
	.owner = THIS_MODULE,
	.open = aml_kt_open,
	.release = aml_kt_release,
	.unlocked_ioctl = aml_kt_ioctl,
	.compat_ioctl = aml_kt_ioctl,
};

static int aml_kt_get_dts_info(struct aml_kt_dev *dev, struct platform_device *pdev)
{
	int ret = 0;
	u32 offset[7];
	u32 cap[6];

	if (unlikely(!dev)) {
		LOGE("Empty aml_kt_dev\n");
		return KT_ERROR;
	}

	/* kt register offset */
	if (of_property_read_u32_array(pdev->dev.of_node, "kt_offset", offset,
			ARRAY_SIZE(offset)) == 0) {
		dev->reg.rdy_offset = offset[0];
		dev->reg.cfg_offset = offset[1];
		dev->reg.sts_offset = offset[2];
		dev->reg.key0_offset = offset[3];
		dev->reg.key1_offset = offset[4];
		dev->reg.key2_offset = offset[5];
		dev->reg.key3_offset = offset[6];
	} else {
		LOGE("%s: not found\n", "kt_offset");
		return KT_ERROR;
	}

	/* kt capability */
	if (of_property_read_u32_array(pdev->dev.of_node, "kt_cap", cap,
			ARRAY_SIZE(cap)) == 0) {
		dev->algo_cap = cap[0];
		dev->user_cap = cap[1];
		dev->kte_start = cap[2];
		dev->kte_end = cap[3];
		dev->ive_start = cap[4];
		dev->ive_end = cap[5];
	} else {
		LOGE("%s: not found\n", "kt_cap");
		return KT_ERROR;
	}

	/* S17 algo cfg */
	if (dev->algo_cap & KT_CAP_S17) {
		ret = of_property_read_u32(pdev->dev.of_node, "s17_cfg_offset",
			&dev->reg.s17_cfg_offset);
		if (ret) {
			LOGE("%s: not found 0x%x\n", "s17_cfg_offset", dev->reg.s17_cfg_offset);
			return KT_ERROR;
		}
	}

	/* Check reserved KTE */
	ret = of_property_read_u32(pdev->dev.of_node, "kt_reserved", &dev->kt_reserved);
	if (ret) {
		LOGE("%s: not found 0x%x\n", "kt_reserved", dev->kt_reserved);
		return KT_ERROR;
	}

	return KT_SUCCESS;
}

int aml_kt_init(struct class *aml_kt_class, struct platform_device *pdev)
{
	int ret = -1;
	struct device *device;
	struct resource *res;

	if (alloc_chrdev_region(&aml_kt_devt, 0, DEVICE_INSTANCES,
				AML_KT_DEVICE_NAME) < 0) {
		LOGE("%s device can't be allocated.\n", AML_KT_DEVICE_NAME);
		return -ENXIO;
	}

	cdev_init(&aml_kt_dev.cdev, &aml_kt_fops);
	aml_kt_dev.cdev.owner = THIS_MODULE;
	ret = cdev_add(&aml_kt_dev.cdev,
		       MKDEV(MAJOR(aml_kt_devt), MINOR(aml_kt_devt)), 1);
	aml_kt_dev.base_addr = NULL;
	if (unlikely(ret < 0))
		goto unregister_chrdev;

	device = device_create(aml_kt_class, NULL, aml_kt_devt, NULL,
			       AML_KT_DEVICE_NAME);
	if (IS_ERR(device)) {
		LOGE("device_create failed\n");
		ret = PTR_ERR(device);
		goto delete_cdev;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		LOGE("%s: platform_get_resource is failed\n", __func__);
		ret = -ENOMEM;
		goto destroy_device;
	}

	aml_kt_dev.base_addr = devm_ioremap_resource(&pdev->dev, res);
	if (!aml_kt_dev.base_addr) {
		LOGE("%s base addr error\n", __func__);
		ret = -ENOMEM;
		goto destroy_device;
	}

	ret = aml_kt_get_dts_info(&aml_kt_dev, pdev);
	if (ret != KT_SUCCESS) {
		LOGE("%s: cannot find match dts info\n", __func__);
		ret = -EINVAL;
		goto destroy_device;
	}

	mutex_init(&aml_kt_dev.lock);
	memset(aml_kt_dev.kt_slot, 0, sizeof(aml_kt_dev.kt_slot));
	memset(aml_kt_dev.kt_iv_slot, 0, sizeof(aml_kt_dev.kt_iv_slot));
	return ret;

destroy_device:
	device_destroy(aml_kt_class, aml_kt_devt);
delete_cdev:
	cdev_del(&aml_kt_dev.cdev);
unregister_chrdev:
	unregister_chrdev_region(aml_kt_devt, DEVICE_INSTANCES);

	return ret;
}

void aml_kt_exit(struct class *aml_kt_class, struct platform_device *pdev)
{
	int i = 0;

	for (i = aml_kt_dev.kte_start; i < aml_kt_dev.kte_end; i++) {
		if (aml_kt_dev.kt_reserved && aml_kt_slot_reserved(i))
			continue;

		kfree(aml_kt_dev.kt_slot[i]);
		aml_kt_dev.kt_slot[i] = NULL;
	}

	for (i = aml_kt_dev.ive_start; i < aml_kt_dev.ive_end; i++) {
		kfree(aml_kt_dev.kt_iv_slot[i]);
		aml_kt_dev.kt_iv_slot[i] = NULL;
	}

	device_destroy(aml_kt_class, aml_kt_devt);
	cdev_del(&aml_kt_dev.cdev);
	unregister_chrdev_region(MKDEV(MAJOR(aml_kt_devt),
				       MINOR(aml_kt_devt)),
				 DEVICE_INSTANCES);
	mutex_destroy(&aml_kt_dev.lock);
}

static int aml_kt_probe(struct platform_device *pdev)
{
	int ret = 0;

	aml_kt_class = class_create(THIS_MODULE, AML_KT_DEVICE_NAME);
	if (IS_ERR(aml_kt_class)) {
		LOGE("class_create failed\n");
		ret = PTR_ERR(aml_kt_class);
		return ret;
	}

	ret = aml_kt_init(aml_kt_class, pdev);
	if (unlikely(ret < 0)) {
		LOGE("aml_kt_init failed\n");
		goto destroy_class;
	}

	ret = aml_kt_init_dbgfs();
	if (ret) {
		LOGE("aml_kt_init_dbgfs failed\n");
		goto destroy_class;
	}

	return 0;

destroy_class:
	class_destroy(aml_kt_class);
	return ret;
}

static int aml_kt_remove(struct platform_device *pdev)
{
	aml_kt_exit(aml_kt_class, pdev);
	class_destroy(aml_kt_class);
	debugfs_remove_recursive(aml_kt_debug_dent);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id aml_kt_dt_match[] = {
	{
		.compatible = "amlogic,aml_kt"
	},
	{},
};
MODULE_DEVICE_TABLE(of, aml_kt_dt_match);
#else
#define aml_kt_dt_match NULL
#endif

static struct platform_driver aml_kt_drv = {
	.probe = aml_kt_probe,
	.remove = aml_kt_remove,
	.driver = {
		.name = AML_KT_DEVICE_NAME,
		.of_match_table = aml_kt_dt_match,
		.owner = THIS_MODULE,
	},
};

module_platform_driver(aml_kt_drv);
MODULE_AUTHOR("Amlogic Inc.");
MODULE_DESCRIPTION("AML Keytable device driver");
MODULE_LICENSE("GPL v2");
