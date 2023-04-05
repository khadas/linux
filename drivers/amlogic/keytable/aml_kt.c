// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/aml_kt.h>

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/amlogic/iomap.h>
#include "aml_kt_log.h"

#define DEVICE_NAME "aml_kt"
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
#define KT_IV_FLAG_OFFSET (31)

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
#define KT_KTE_MAX (128)

struct aml_kt_dev {
	struct cdev cdev;
	struct mutex lock; /*define mutex*/
	struct amlkt_cfg_param *kt_slot[KT_KTE_MAX];
	void __iomem *base_addr;
	struct reg {
		u32 rdy_offset;
		u32 cfg_offset;
		u32 sts_offset;
		u32 key0_offset;
		u32 key1_offset;
		u32 key2_offset;
		u32 key3_offset;
		u32 s17_cfg_offset;
	} reg;
	u32 user_cap;
	u32 algo_cap;
	u32 kte_start;
	u32 kte_end;
};

static dev_t aml_kt_devt;
static struct aml_kt_dev aml_kt_dev;
static struct class *aml_kt_class;

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

__attribute__((always_inline)) static inline u32
aml_kt_ioread32(void __iomem *base_addr, u32 offset)
{
	u32 val;

	val = ioread32((char *)base_addr + offset);

	return val;
}

__attribute__((always_inline)) static inline void
aml_kt_iowrite32(u32 data, void __iomem *base_addr, u32 offset)
{
	iowrite32(data, (char *)base_addr + offset);
}

static int aml_kt_lock(struct aml_kt_dev *dev)
{
	int cnt = 0;
	int ret = KT_SUCCESS;

	mutex_lock(&dev->lock);
	while (aml_kt_ioread32(dev->base_addr, dev->reg.rdy_offset) == 0) {
		if (cnt++ > KT_PENDING_WAIT_TIMEOUT) {
			LOGE("Error: wait KT ready timeout\n");
			ret = KT_ERROR;
		}
	}

	mutex_unlock(&dev->lock);
	return ret;
}

static void aml_kt_unlock(struct aml_kt_dev *dev)
{
	mutex_lock(&dev->lock);
	aml_kt_iowrite32(1, dev->base_addr, dev->reg.rdy_offset);
	mutex_unlock(&dev->lock);
}

static int aml_kt_read_pending(struct aml_kt_dev *dev)
{
	int ret = KT_ERROR;
	int cnt = 0;
	u32 reg_ret = 0;

	do {
		reg_ret = aml_kt_ioread32(dev->base_addr, dev->reg.cfg_offset);
		if (cnt++ > KT_PENDING_WAIT_TIMEOUT) {
			LOGE("Error: wait KT pending done timeout\n");
			ret = KT_ERROR;
			return ret;
		}
	} while (reg_ret & (KT_PENDING << KTE_PENDING_OFFSET));

	reg_ret = (reg_ret >> KTE_STATUS_OFFSET) & KTE_STATUS_MASK;
	if (reg_ret != KT_STS_OK) {
		LOGE("Error: KT return error:%#x\n", reg_ret);
		ret = KT_ERROR;
	} else {
		ret = KT_SUCCESS;
	}

	return ret;
}

static int aml_kt_handle_to_kte(struct aml_kt_dev *dev, u32 handle, u32 *kte)
{
	int ret = KT_SUCCESS;

	if (unlikely(!dev)) {
		LOGE("Empty aml_kt_dev\n");
		return KT_ERROR;
	}

	*kte = handle & ~(1 << KT_IV_FLAG_OFFSET);
	if (*kte >= dev->kte_end) {
		LOGE("Exceed kt max slot\n");
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

static int aml_kt_get_flag(struct aml_kt_dev *dev, u32 handle)
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

	if (aml_kt_lock(dev) != KT_SUCCESS) {
		LOGE("lock error\n");
		ret = KT_ERROR;
		goto exit;
	}

	reg_offset = dev->reg.cfg_offset;
	reg_val = (KT_PENDING << KTE_PENDING_OFFSET |
			KT_MODE_READ << KTE_MODE_OFFSET |
			kte << KTE_KTE_OFFSET);
	aml_kt_iowrite32(reg_val, dev->base_addr, reg_offset);

	if (aml_kt_read_pending(dev) != KT_SUCCESS) {
		LOGE("pending error kte[%d]\n", kte);
		ret = KT_ERROR;
		goto exit;
	}

	reg_ret = aml_kt_ioread32(dev->base_addr, dev->reg.sts_offset);
	if (reg_ret != KT_ERROR) {
		LOGD("kte=%d KT_REE_STS=0x%08x\n", kte, reg_ret);
		ret = KT_SUCCESS;
		goto exit;
	}

exit:
	aml_kt_unlock(dev);

	return ret;
}

static int aml_kt_clean_kte(struct aml_kt_dev *dev, u32 kte)
{
	int ret = 0;
	u32 reg_val = 0;
	u32 reg_offset = 0;

	if (aml_kt_lock(dev) != KT_SUCCESS)
		return KT_ERROR;

	reg_offset = dev->reg.cfg_offset;
	reg_val = (KT_PENDING << KTE_PENDING_OFFSET |
			KT_CLEAN_KTE << KTE_CLEAN_OFFSET |
			KT_MODE_HOST << KTE_MODE_OFFSET |
			kte << KTE_KTE_OFFSET);
	aml_kt_iowrite32(reg_val, dev->base_addr, reg_offset);

	if (aml_kt_read_pending(dev) != KT_SUCCESS) {
		LOGE("pending error\n");
		ret = KT_ERROR;
		goto exit;
	}

exit:
	aml_kt_unlock(dev);

	return ret;
}

static int aml_kt_alloc(struct file *filp, u32 flag, u32 *handle)
{
	struct aml_kt_dev *dev = filp->private_data;
	int res = KT_SUCCESS;
	int i;
	u32 entry_start;
	u32 entry_end;
	u32 slot_size;
	u8 is_iv = 0;

	if (unlikely(!dev)) {
		LOGE("Empty aml_kt_dev\n");
		return KT_ERROR;
	}

	if (!handle) {
		LOGE("handle is null\n");
		return KT_ERROR;
	}

	if (flag == AML_KT_ALLOC_FLAG_IV)
		is_iv = 1;

	entry_start = dev->kte_start;
	entry_end = dev->kte_end;
	slot_size = entry_end - entry_start;
	mutex_lock(&dev->lock);
	for (i = 0; i < slot_size; i++) {
		if (dev->kt_slot[i])
			continue;

		/* Skip reserved key slots */
		if ((get_cpu_type() == MESON_CPU_MAJOR_ID_C3) && aml_kt_slot_reserved(i))
			continue;

		dev->kt_slot[i] = kzalloc(sizeof(*dev->kt_slot[i]), GFP_KERNEL);
		if (!dev->kt_slot[i]) {
			LOGE("Error: KT kzalloc failed\n");
			res = KT_ERROR;
			goto exit;
		}

		*handle = i | (is_iv << KT_IV_FLAG_OFFSET);
		break;
	}

	LOGI("flag:%#x, is_iv:%d, handle:%#x, index:%d\n", flag, is_iv, *handle, i);
	if (i == entry_end) {
		LOGE("Error: KT alloc return error, no kte available\n");
		res = KT_ERROR;
	} else {
		res = KT_SUCCESS;
	}

exit:
	mutex_unlock(&dev->lock);

	return res;
}

static int aml_kt_config(struct file *filp, struct amlkt_cfg_param key_cfg)
{
	struct aml_kt_dev *dev = filp->private_data;
	int ret = KT_SUCCESS;
	u32 index;
	u8 is_iv = 0;

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
	LOGI("handle:%#x, index:%d\n", key_cfg.handle, index);

	/* Conversion T5W key algorithm */
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_T5W) {
		if (key_cfg.key_algo == AML_KT_ALGO_AES)
			key_cfg.key_algo = 2;
		if (key_cfg.key_algo == AML_KT_ALGO_DES)
			key_cfg.key_algo = 0;
	}

	/* Check Invalid key algorithm */
	LOGD("dev->algo_cap:0x%x\n", dev->algo_cap);
	LOGD("dev->user_cap:0x%x\n", dev->user_cap);
	switch (key_cfg.key_algo) {
	case AML_KT_ALGO_AES:
		if (!(dev->algo_cap & KT_CAP_AES)) {
			LOGE("invalid key algorithm:%d\n", key_cfg.key_algo);
			return KT_ERROR;
		}
		break;
	case AML_KT_ALGO_TDES:
		if (!(dev->algo_cap & KT_CAP_TDES)) {
			LOGE("invalid key algorithm:%d\n", key_cfg.key_algo);
			return KT_ERROR;
		}
		break;
	case AML_KT_ALGO_DES:
		if (!(dev->algo_cap & KT_CAP_DES)) {
			LOGE("invalid key algorithm:%d\n", key_cfg.key_algo);
			return KT_ERROR;
		}
		break;
	case AML_KT_ALGO_S17:
		if (!(dev->algo_cap & KT_CAP_S17)) {
			LOGE("invalid key algorithm:%d\n", key_cfg.key_algo);
			return KT_ERROR;
		}
		break;
	case AML_KT_ALGO_SM4:
		if (!(dev->algo_cap & KT_CAP_SM4)) {
			LOGE("invalid key algorithm:%d\n", key_cfg.key_algo);
			return KT_ERROR;
		}
		break;
	case AML_KT_ALGO_NDL:
		if (!(dev->algo_cap & KT_CAP_NDL)) {
			LOGE("invalid key algorithm:%d\n", key_cfg.key_algo);
			return KT_ERROR;
		}
		break;
	case AML_KT_ALGO_ND:
		if (!(dev->algo_cap & KT_CAP_ND)) {
			LOGE("invalid key algorithm:%d\n", key_cfg.key_algo);
			return KT_ERROR;
		}
		break;
	case AML_KT_ALGO_CSA3:
		if (!(dev->algo_cap & KT_CAP_CSA3)) {
			LOGE("invalid key algorithm:%d\n", key_cfg.key_algo);
			return KT_ERROR;
		}
		break;
	case AML_KT_ALGO_CSA2:
		if (!(dev->algo_cap & KT_CAP_CSA2)) {
			LOGE("invalid key algorithm:%d\n", key_cfg.key_algo);
			return KT_ERROR;
		}
		break;
	case AML_KT_ALGO_HMAC:
		if (!(dev->algo_cap & KT_CAP_HMAC)) {
			LOGE("invalid key algorithm:%d\n", key_cfg.key_algo);
			return KT_ERROR;
		}
		break;
	default:
		LOGE("invalid key algorithm:%d\n", key_cfg.key_algo);
		return KT_ERROR;
	}

	/* Check Invalid key user id */
	switch (key_cfg.key_userid) {
	case AML_KT_USER_M2M_0:
		if (!(dev->user_cap & KT_CAP_M2M_0)) {
			LOGE("invalid key user:%d\n", key_cfg.key_userid);
			return KT_ERROR;
		}
		break;
	case AML_KT_USER_M2M_1:
		if (!(dev->user_cap & KT_CAP_M2M_1)) {
			LOGE("invalid key user:%d\n", key_cfg.key_userid);
			return KT_ERROR;
		}
		break;
	case AML_KT_USER_M2M_2:
		if (!(dev->user_cap & KT_CAP_M2M_2)) {
			LOGE("invalid key user:%d\n", key_cfg.key_userid);
			return KT_ERROR;
		}
		break;
	case AML_KT_USER_M2M_3:
		if (!(dev->user_cap & KT_CAP_M2M_3)) {
			LOGE("invalid key user:%d\n", key_cfg.key_userid);
			return KT_ERROR;
		}
		break;
	case AML_KT_USER_M2M_4:
		if (!(dev->user_cap & KT_CAP_M2M_4)) {
			LOGE("invalid key user:%d\n", key_cfg.key_userid);
			return KT_ERROR;
		}
		break;
	case AML_KT_USER_M2M_5:
		if (!(dev->user_cap & KT_CAP_M2M_5)) {
			LOGE("invalid key user:%d\n", key_cfg.key_userid);
			return KT_ERROR;
		}
		break;
	case AML_KT_USER_M2M_ANY:
		if (!(dev->user_cap & KT_CAP_M2M_ANY)) {
			LOGE("invalid key user:%d\n", key_cfg.key_userid);
			return KT_ERROR;
		}
		break;
	case AML_KT_USER_TSD:
		if (!(dev->user_cap & KT_CAP_TSD)) {
			LOGE("invalid key user:%d\n", key_cfg.key_userid);
			return KT_ERROR;
		}
		break;
	case AML_KT_USER_TSN:
		if (!(dev->user_cap & KT_CAP_TSN)) {
			LOGE("invalid key user:%d\n", key_cfg.key_userid);
			return KT_ERROR;
		}
		break;
	case AML_KT_USER_TSE:
		if (!(dev->user_cap & KT_CAP_TSE)) {
			LOGE("invalid key user:%d\n", key_cfg.key_userid);
			return KT_ERROR;
		}
		break;
	default:
		LOGE("invalid key user:%d\n", key_cfg.key_userid);
		return KT_ERROR;
	}

	/* Special case for S17 key algorithm */
	if (key_cfg.key_algo == AML_KT_ALGO_S17 && is_iv == 0) {
		if (key_cfg.ext_value == 0) {
			aml_kt_iowrite32(S17_CFG_DEFAULT, dev->base_addr, dev->reg.s17_cfg_offset);
			LOGD("default frobenius :0x%0x\n", S17_CFG_DEFAULT);
		} else {
			aml_kt_iowrite32(key_cfg.ext_value, dev->base_addr,
					 dev->reg.s17_cfg_offset);
			LOGD("frobenius :0x%0x\n", key_cfg.ext_value);
		}
	}

	if (!dev->kt_slot[index]) {
		LOGE("kt_slot is null\n");
		return KT_ERROR;
	}
	memcpy(dev->kt_slot[index], &key_cfg, sizeof(struct amlkt_cfg_param));

	return ret;
}

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
	aml_kt_iowrite32(reg_val, dev->base_addr, reg_offset);

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
	aml_kt_iowrite32(tmp_key, dev->base_addr, offset);
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

	if (aml_kt_lock(dev) != KT_SUCCESS) {
		LOGE("lock error\n");
		return KT_ERROR;
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
			goto exit;
		} else {
			ret = KT_SUCCESS;
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
			goto exit;
		} else {
			ret = KT_SUCCESS;
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
			goto exit;
		} else {
			ret = KT_SUCCESS;
		}
	} else {
		LOGE("Error: unsupported keylen:%d\n", keylen);
		ret = KT_ERROR;
		goto exit;
	}

exit:
	aml_kt_unlock(dev);

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

	if (aml_kt_lock(dev) != KT_SUCCESS) {
		LOGE("lock error\n");
		return KT_ERROR;
	}

	LOGD("--------------------------------------------------------------\n");
	LOGD("flag:%d, algo:%d, uid:%d, src:%d\n", key_cfg->key_flag, key_cfg->key_algo,
	     key_cfg->key_userid, key_cfg->key_source);

	ret = aml_kt_write_cfg(dev, handle, key_mode, key_cfg, func_id);
	if (ret == KT_ERROR) {
		LOGE("aml_kt_write_cfg error\n");
		goto exit;
	} else {
		ret = KT_SUCCESS;
	}

exit:
	aml_kt_unlock(dev);

	return ret;
}

static int aml_kt_set_host_key(struct file *filp, struct amlkt_set_key_param *key_param)
{
	struct aml_kt_dev *dev = filp->private_data;
	int ret = KT_SUCCESS;
	u32 index = 0;
	u32 key_source = 0;

	ret = aml_kt_handle_to_kte(dev, key_param->handle, &index);
	if (ret != KT_SUCCESS) {
		LOGE("index get error\n");
		return ret;
	}
	LOGI("handle:%#x, index:%d\n", key_param->handle, index);

	if (!dev->kt_slot[index]) {
		ret = KT_ERROR;
		return ret;
	}

	key_source = dev->kt_slot[index]->key_source;
	switch (key_source) {
	case AML_KT_SRC_REE_HOST:
		ret = aml_kt_set_inter_key(dev, key_param->handle,
						dev->kt_slot[index], key_param->key,
						key_param->key_len, KT_MODE_HOST);
		break;
	default:
		LOGE("Not support key_source[%d]\n", key_source);
		ret = KT_ERROR;
		break;
	}

	if (ret == KT_SUCCESS)
		aml_kt_get_flag(dev, key_param->handle);

	return ret;
}

static int aml_kt_set_hw_key(struct file *filp, u32 handle)
{
	struct aml_kt_dev *dev = filp->private_data;
	int ret = KT_SUCCESS;
	u32 index = 0;
	u32 key_source = 0;

	ret = aml_kt_handle_to_kte(dev, handle, &index);
	if (ret != KT_SUCCESS) {
		LOGE("index get error\n");
		return KT_ERROR;
	}
	LOGI("handle:%#x, index:%d\n", handle, index);

	if (!dev->kt_slot[index]) {
		LOGE("kt_slot is null\n");
		return KT_ERROR;
	}
	key_source = dev->kt_slot[index]->key_source;

	switch (key_source) {
	case AML_KT_SRC_REE_CERT:
	case AML_KT_SRC_REEKL_NAGRA:
		ret = aml_kt_set_inter_hw_key(dev, handle, dev->kt_slot[index],
							KT_MODE_NAGRA);
		break;
	case AML_KT_SRC_NSK:
		ret = aml_kt_set_inter_hw_key(dev, handle, dev->kt_slot[index],
							KT_MODE_NSK);
		break;
	default:
		LOGE("Not support key_source[%d]\n", key_source);
		ret = KT_ERROR;
		break;
	}

	if (ret == KT_SUCCESS)
		aml_kt_get_flag(dev, handle);

	return ret;
}

static int aml_kt_invalidate(struct file *filp, u32 handle)
{
	struct aml_kt_dev *dev = filp->private_data;
	int ret = KT_SUCCESS;
	u32 kte = 0;

	if (unlikely(!dev)) {
		LOGE("Empty aml_kt_dev\n");
		return KT_ERROR;
	}

	ret = aml_kt_handle_to_kte(dev, handle, &kte);
	if (ret != KT_SUCCESS) {
		LOGE("kte get error\n");
		return ret;
	}

	ret = aml_kt_clean_kte(dev, kte);
	if (ret != KT_SUCCESS) {
		LOGE("Error: KT invalidate clean kte error\n");
		ret = KT_ERROR;
	}

	return ret;
}

static int aml_kt_free(struct file *filp, u32 handle)
{
	struct aml_kt_dev *dev = filp->private_data;
	int ret = KT_SUCCESS;
	u32 kte = 0;

	if (unlikely(!dev)) {
		LOGE("Empty aml_kt_dev\n");
		return KT_ERROR;
	}

	ret = aml_kt_handle_to_kte(dev, handle, &kte);
	if (ret != KT_SUCCESS) {
		LOGE("kte get error\n");
		return ret;
	}

	LOGI("handle:%#x, kte:%d\n", handle, kte);
	ret = aml_kt_clean_kte(dev, kte);
	if (ret != KT_SUCCESS) {
		LOGE("Error: KT invalidate clean kte error\n");
		return KT_ERROR;
	}
	mutex_lock(&dev->lock);
	kfree(dev->kt_slot[kte]);
	dev->kt_slot[kte] = NULL;
	mutex_unlock(&dev->lock);

	return ret;
}

int aml_kt_open(struct inode *inode, struct file *filp)
{
	struct aml_kt_dev *dev;

	dev = container_of(inode->i_cdev, struct aml_kt_dev, cdev);
	filp->private_data = dev;

	return 0;
}

int aml_kt_release(struct inode *inode, struct file *filp)
{
	__maybe_unused struct aml_kt_dev *dev =
		container_of(inode->i_cdev, struct aml_kt_dev, cdev);

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
			LOGE("aml_kt_alloc copy error\n");
			return -EFAULT;
		}

		ret = aml_kt_alloc(filp, alloc_param.flag, &alloc_param.handle);
		if (ret != 0) {
			LOGE("aml_kt_alloc failed retval=0x%08x\n", ret);
			return -EFAULT;
		}

		ret = copy_to_user((void __user *)arg, &alloc_param, sizeof(alloc_param));
		if (unlikely(ret)) {
			LOGE("aml_kt_alloc copy error\n");
			return -EFAULT;
		}
		break;
	case AML_KT_CONFIG:
		memset(&cfg_param, 0, sizeof(cfg_param));
		if (copy_from_user(&cfg_param, (void __user *)arg, sizeof(cfg_param))) {
			LOGE("aml_kt_config copy error\n");
			return -EFAULT;
		}

		ret = aml_kt_config(filp, cfg_param);
		if (ret != 0) {
			LOGE("aml_kt_config failed retval=0x%08x\n", ret);
			return -EFAULT;
		}
		break;
	case AML_KT_SET:
		memset(&key_param, 0, sizeof(key_param));
		if (copy_from_user(&key_param, (void __user *)arg, sizeof(key_param))) {
			LOGE("aml_kt_set_host_key copy error\n");
			return -EFAULT;
		}

		ret = aml_kt_set_host_key(filp, &key_param);
		if (ret != 0) {
			LOGE("aml_kt_set failed retval=0x%08x\n", ret);
			return -EFAULT;
		}
		break;
	case AML_KT_HW_SET:
		ret = get_user(handle, (u32 __user *)arg);
		if (unlikely(ret)) {
			LOGE("aml_kt_set_hw_key copy error\n");
			return ret;
		}

		ret = aml_kt_set_hw_key(filp, handle);
		if (ret != 0) {
			LOGE("aml_kt_hw_set failed retval=0x%08x\n", ret);
			return -EFAULT;
		}
		break;
	case AML_KT_FREE:
		ret = get_user(handle, (u32 __user *)arg);
		if (unlikely(ret)) {
			LOGE("aml_kt_free copy error\n");
			return ret;
		}

		ret = aml_kt_free(filp, handle);
		if (ret != 0) {
			LOGE("aml_kt_free failed retval=0x%08x\n", ret);
			return -EFAULT;
		}
		break;
	case AML_KT_INVALIDATE:
		ret = get_user(handle, (u32 __user *)arg);
		if (unlikely(ret)) {
			LOGE("aml_kt_invalidate copy error\n");
			return ret;
		}

		ret = aml_kt_invalidate(filp, handle);
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
	u32 cap[4];

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
	} else {
		LOGE("%s: not found\n", "kt_cap");
		return KT_ERROR;
	}

	/* S4D s17 algo cfg */
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_S4D) {
		ret = of_property_read_u32(pdev->dev.of_node, "s17_cfg_offset",
			&dev->reg.s17_cfg_offset);
		if (ret) {
			LOGE("%s: not found 0x%x\n", "s17_cfg_offset", dev->reg.s17_cfg_offset);
			return KT_ERROR;
		}
	}

	return KT_SUCCESS;
}

int aml_kt_init(struct class *aml_kt_class, struct platform_device *pdev)
{
	int ret = -1;
	struct device *device;
	const struct of_device_id *match;
	struct resource *res;

	if (alloc_chrdev_region(&aml_kt_devt, 0, DEVICE_INSTANCES,
				DEVICE_NAME) < 0) {
		LOGE("%s device can't be allocated.\n", DEVICE_NAME);
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
			       DEVICE_NAME);
	if (IS_ERR(device)) {
		LOGE("device_create failed\n");
		ret = PTR_ERR(device);
		goto delete_cdev;
	}

	match = of_match_device(aml_kt_dt_match, &pdev->dev);
	if (!match) {
		LOGE("%s: cannot find match dt\n", __func__);
		ret = -EINVAL;
		goto destroy_device;
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
	device_destroy(aml_kt_class, aml_kt_devt);
	cdev_del(&aml_kt_dev.cdev);
	unregister_chrdev_region(MKDEV(MAJOR(aml_kt_devt),
				       MINOR(aml_kt_devt)),
				 DEVICE_INSTANCES);
	mutex_destroy(&aml_kt_dev.lock);
	LOGI("%s module has been unloaded.\n", DEVICE_NAME);
}

static int aml_kt_probe(struct platform_device *pdev)
{
	int ret = 0;

	aml_kt_class = class_create(THIS_MODULE, DEVICE_NAME);
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

	LOGI("%s module has been loaded.\n", DEVICE_NAME);
	return 0;

destroy_class:
	class_destroy(aml_kt_class);
	return ret;
}

static int aml_kt_remove(struct platform_device *pdev)
{
	aml_kt_exit(aml_kt_class, pdev);
	class_destroy(aml_kt_class);
	LOGI("%s module has been unloaded.\n", KBUILD_MODNAME);
	return 0;
}

static struct platform_driver aml_kt_drv = {
	.probe = aml_kt_probe,
	.remove = aml_kt_remove,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = aml_kt_dt_match,
		.owner = THIS_MODULE,
	},
};

module_platform_driver(aml_kt_drv);
MODULE_AUTHOR("Amlogic Inc.");
MODULE_DESCRIPTION("AML Keytable device driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(MODULES_KT_VERSION_BASE);
