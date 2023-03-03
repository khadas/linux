// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/aml_mkl.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/printk.h>
#include <linux/mutex.h>
#include <linux/amlogic/iomap.h>
#include "aml_mkl_log.h"

#define DEVICE_NAME "aml_mkl"
#define DEVICE_INSTANCES 1

/* kl offset */
#define KL_PENDING_OFFSET (31U)
#define KL_PENDING_MASK (1)
#define KL_STATUS_OFFSET (29)
#define KL_STATUS_MASK (3)
#define KL_ALGO_OFFSET (28)
#define KL_ALGO_MASK (1)
#define KL_MODE_OFFSET (26)
#define KL_MODE_MASK (3)
#define KL_FLAG_OFFSET (24)
#define KL_FLAG_MASK (3)
#define KL_KEYALGO_OFFSET (20)
#define KL_KEYALGO_MASK (0xf)
#define KL_USERID_OFFSET (16)
#define KL_USERID_MASK (0xf)
#define KL_KTE_OFFSET (8)
#define KL_KTE_MASK (0xff)
#define KL_MRK_OFFSET (4)
#define KL_MRK_MASK (0xf)
#define KL_FUNC_ID_OFFSET (0)
#define KL_FUNC_ID_MASK (0xf)
#define KL_MID_OFFSET (24)
#define KL_MID_MASK (0xff)
#define KL_MID_EXTRA_OFFSET (8)
#define KL_TEE_PRIV_OFFSET (7)
#define KL_TEE_PRIV_MASK (1)
#define KL_TEE_SEP_OFFSET (2)
#define KL_TEE_SEP_MASK (3)
#define KL_LEVEL_OFFSET (4)
#define KL_LEVEL_MASK (7)
#define KL_STAGE_OFFSET (0)
#define KL_STAGE_MASK (3)

#define KL_MSR_FUNCID_OFFSET  (0)
#define KL_MSR_KEYALGO_OFFSET (12)
#define KL_MSR_PAYLOAD_OFFSET (16)
#define KL_MSR_BUFLEVEL_OFFSET (4)

/* kl etc */
#define KT_KTE_MAX (256)
#define HANDLE_TO_KTE(h) ((h) & (KT_KTE_MAX - 1))
#define KL_PENDING_WAIT_TIMEOUT (20000)
#define KL_ETSI_MID_MSG (0)

/* kl status */
#define KL_STATUS_OK (0)
#define KL_STATUS_ERROR_PERMISSION_DINED (1)
#define KL_STATUS_ERROR_OTP (2)
#define KL_STATUS_ERROR_DEPOSIT (3)
#define KL_STATUS_ERROR_TIMEOUT (4)
#define KL_STATUS_ERROR_BAD_PARAM (5)
#define KL_STATUS_ERROR_BAD_STATE (6)

struct aml_mkl_dev {
	struct cdev cdev;
	struct mutex lock; /*define mutex*/
	void __iomem *base_addr;
	union {
		struct reg {
			u32 rdy_offset;
			u32 cfg_offset;
			u32 cmd_offset;
			u32 ek_offset;
		} reg;
		struct t5w_reg {
			u32 start0_offset;
			u32 key1_offset;
			u32 nonce_offset;
			u32 key2_offset;
			u32 key3_offset;
			u32 key4_offset;
			u32 key5_offset;
			u32 key6_offset;
			u32 key7_offset;
		} t5w_reg;
	};
};

static dev_t aml_mkl_devt;
static struct aml_mkl_dev aml_mkl_dev;
static struct class *aml_mkl_class;

__attribute__((always_inline)) static inline u32
aml_mkl_ioread32(void __iomem *base_addr, u32 offset)
{
	u32 val;

	val = ioread32((char *)base_addr + offset);

	return val;
}

__attribute__((always_inline)) static inline void
aml_mkl_iowrite32(u32 data, void __iomem *base_addr, u32 offset)
{
	iowrite32(data, (char *)base_addr + offset);
}

static int aml_mkl_open(struct inode *inode, struct file *file)
{
	struct aml_mkl_dev *dev;

	dev = container_of(inode->i_cdev, struct aml_mkl_dev, cdev);
	file->private_data = dev;

	return 0;
}

static int aml_mkl_release(struct inode *inode, struct file *file)
{
	__maybe_unused struct aml_mkl_dev *dev =
		container_of(inode->i_cdev, struct aml_mkl_dev, cdev);

	if (!file->private_data)
		return 0;

	file->private_data = NULL;
	return 0;
}

static int aml_mkl_program_key(void __iomem *base_addr, u32 offset, const u8 *data)
{
	int i = 0;
	const u32 *data32 = (const u32 *)data;

	if (!data32)
		return -1;

	for (i = 0; i < 4; i++)
		aml_mkl_iowrite32(data32[i], base_addr, offset + i * sizeof(u32));

	return 0;
}

static int aml_mkl_lock(struct aml_mkl_dev *dev)
{
	int cnt = 0;

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_T5W) {
		while ((aml_mkl_ioread32(dev->base_addr, dev->t5w_reg.start0_offset) >> 31) & 1) {
			if (cnt++ > KL_PENDING_WAIT_TIMEOUT) {
				LOGE("Error: wait KL ready timeout\n");
				return KL_STATUS_ERROR_TIMEOUT;
			}
		}
	} else {
		while (aml_mkl_ioread32(dev->base_addr, dev->reg.rdy_offset) != 1) {
			if (cnt++ > KL_PENDING_WAIT_TIMEOUT) {
				LOGE("Error: wait KL ready timeout\n");
				return KL_STATUS_ERROR_TIMEOUT;
			}
		}
	}

	return KL_STATUS_OK;
}

static void aml_mkl_unlock(struct aml_mkl_dev *dev)
{
	aml_mkl_iowrite32(1, dev->base_addr, dev->reg.rdy_offset);
}

static int aml_mkl_read_pending(struct aml_mkl_dev *dev)
{
	int ret = -1;
	int cnt = 0;
	u32 reg_ret = 0;

	do {
		reg_ret = aml_mkl_ioread32(dev->base_addr, dev->reg.cfg_offset);
		if (cnt++ > KL_PENDING_WAIT_TIMEOUT) {
			LOGE("Error: wait KL pending done timeout\n");
			ret = KL_STATUS_ERROR_TIMEOUT;
			return ret;
		}
	} while (reg_ret & (1 << KL_PENDING_OFFSET));

	reg_ret = (reg_ret >> KL_STATUS_OFFSET) & KL_STATUS_MASK;
	switch (reg_ret) {
	case 0:
		ret = KL_STATUS_OK;
		break;
	case 1:
		LOGE("Permission Denied Error code: %d\n", ret);
		ret = KL_STATUS_ERROR_BAD_STATE;
		break;
	case 2:
		LOGE("OTP Error code: %d\n", ret);
		ret = KL_STATUS_ERROR_OTP;
		break;
	case 3:
		LOGE("KL Deposit Error code: %d\n", ret);
		ret = KL_STATUS_ERROR_DEPOSIT;
		break;
	}

	return ret;
}

static int aml_mkl_etsi_run(struct file *filp, struct amlkl_params *param)
{
	int ret;
	int i;
	int tee_priv = 0;
	u32 reg_val = 0;
	u32 reg_offset = 0;
	struct amlkl_usage *pu;
	struct aml_mkl_dev *dev = filp->private_data;

	mutex_lock(&dev->lock);

	if (!param) {
		LOGE("Error: param data has Null\n");
		ret = KL_STATUS_ERROR_BAD_PARAM;
		goto exit;
	}

	pu = &param->usage;
	LOGI("kte:%d, levels:%d, mid:%#x, kl_algo:%d, mrk:%d\n",
		param->kt_handle, param->levels, param->module_id,
		param->kl_algo, param->mrk_cfg_index);
	LOGI("kt usage, crypto:%d, algo:%d, uid:%d\n", pu->crypto, pu->algo,
		pu->uid);

	if (pu->uid & ~KL_USERID_MASK ||
	    (pu->uid > AML_KT_USER_M2M_5 && pu->uid < AML_KT_USER_TSD) ||
	    (pu->uid > AML_KT_USER_TSE && pu->uid < KL_USERID_MASK) ||
	    pu->algo & ~KL_KEYALGO_MASK ||
	    (pu->algo > AML_KT_ALGO_DES && pu->algo < AML_KT_ALGO_NDL) ||
	    (pu->algo > AML_KT_ALGO_CSA2 && pu->algo < AML_KT_ALGO_HMAC) ||
	    (pu->algo > AML_KT_ALGO_HMAC && pu->algo < KL_KEYALGO_MASK) ||
	    pu->crypto & ~KL_FLAG_MASK ||
	    param->levels < AML_KL_LEVEL_3 || param->levels > AML_KL_LEVEL_6 ||
	    param->kl_algo > AML_KL_ALGO_AES ||
	    param->mrk_cfg_index > AML_KL_MRK_ETSI_3) {
		ret = KL_STATUS_ERROR_BAD_PARAM;
		goto exit;
	}

	/* 1. Read KL_REE_RDY to lock KL */
	if (aml_mkl_lock(dev) != KL_STATUS_OK) {
		LOGE("key ladder not ready\n");
		ret = KL_STATUS_ERROR_BAD_STATE;
		goto exit;
	}

	/* 2. Program Eks */
	for (i = 0; i < param->levels; i++) {
		ret = aml_mkl_program_key(dev->base_addr, dev->reg.ek_offset + i * 16,
					  param->eks[param->levels - 1 - i]);
		if (ret != 0) {
			LOGE("Error: Ek data has bad parameter\n");
			ret = KL_STATUS_ERROR_BAD_PARAM;
			goto exit;
		}
	}

	/* 3. Program KL_REE_MID */
	reg_val = 0;
	reg_offset = dev->reg.cmd_offset;
	reg_val = (param->module_id << KL_MID_OFFSET |
		   KL_ETSI_MID_MSG << KL_MID_EXTRA_OFFSET |
		   tee_priv << KL_TEE_PRIV_OFFSET);
	aml_mkl_iowrite32(reg_val, dev->base_addr, reg_offset);

	/* 4. Program KL_REE_CFG with KL_pending to 1 */
	reg_val = 0;
	reg_offset = dev->reg.cfg_offset;
	reg_val = (1 << KL_PENDING_OFFSET | param->kl_algo << KL_ALGO_OFFSET |
		   param->kl_mode << KL_MODE_OFFSET |
		   param->usage.crypto << KL_FLAG_OFFSET |
		   param->usage.algo << KL_KEYALGO_OFFSET |
		   param->usage.uid << KL_USERID_OFFSET |
		   (HANDLE_TO_KTE(param->kt_handle)) << KL_KTE_OFFSET |
		   param->mrk_cfg_index << KL_MRK_OFFSET | param->func_id);
	aml_mkl_iowrite32(reg_val, dev->base_addr, reg_offset);

	/* 5. Poll KL_REE_CFG till KL_pending is 0 */
	ret = aml_mkl_read_pending(dev);
	if (ret == KL_STATUS_OK)
		LOGI("ETSI Key Ladder run success\n");

exit:
	mutex_unlock(&dev->lock);
	aml_mkl_unlock(dev);
	return ret;
}

static int aml_mkl_etsi_t5w_run(struct file *filp, struct amlkl_params *param)
{
	int ret;
	int i;
	u32 reg_val = 0;
	u32 reg_offset = 0;
	u32 key_addrs[7] = {0};
	u8 zero[16] = {0};
	struct aml_mkl_dev *dev = filp->private_data;

	mutex_lock(&dev->lock);
	if (!param) {
		LOGE("Error: param data has Null\n");
		ret = KL_STATUS_ERROR_BAD_PARAM;
		goto exit;
	}

	LOGI("kte:%d, levels:%d, kl_num:%d\n", param->kt_handle, param->levels, param->kl_num);

	if (param->levels < AML_KL_LEVEL_3 ||
		param->levels > AML_KL_LEVEL_6 ||
	    param->kl_num > 10) {
		LOGE("Error: param data has bad parameter\n");
		ret = KL_STATUS_ERROR_BAD_PARAM;
		goto exit;
	}

	key_addrs[0] = dev->t5w_reg.key1_offset;
	key_addrs[1] = dev->t5w_reg.key2_offset;
	key_addrs[2] = dev->t5w_reg.key3_offset;
	key_addrs[3] = dev->t5w_reg.key4_offset;
	key_addrs[4] = dev->t5w_reg.key5_offset;
	key_addrs[5] = dev->t5w_reg.key6_offset;
	key_addrs[6] = dev->t5w_reg.key7_offset;

	/* 1. Program Eks */
	ret = aml_mkl_program_key(dev->base_addr, dev->t5w_reg.nonce_offset, zero);
	for (i = 0; i < param->levels; i++) {
		ret = aml_mkl_program_key(dev->base_addr, key_addrs[i],
					param->eks[param->levels - 1 - i]);
	}
	for (i = param->levels; i < 7; i++)
		ret = aml_mkl_program_key(dev->base_addr, key_addrs[i], zero);
	if (ret != 0) {
		LOGE("Error: Ek data has bad parameter\n");
		ret = KL_STATUS_ERROR_BAD_PARAM;
		goto exit;
	}

	/* 2. Program KL_REE_CFG */
	reg_val = 0;
	reg_offset = dev->t5w_reg.start0_offset;
	reg_val = (param->kl_num << 24 | 0 << 22 |
		   (HANDLE_TO_KTE(param->kt_handle)) << 16 |
		   param->reserved[1] << 8 | param->reserved[0]);
	aml_mkl_iowrite32(reg_val, dev->base_addr, reg_offset);

	/* 3. Wait Busy Done */
	if (aml_mkl_lock(dev) != KL_STATUS_OK) {
		LOGE("key ladder is busy\n");
		ret = KL_STATUS_ERROR_BAD_STATE;
		goto exit;
	}

	LOGI("ETSI T5W Key Ladder run success\n");

exit:
	mutex_unlock(&dev->lock);
	return ret;
}

static int aml_mkl_run(struct file *filp, struct amlkl_params *param)
{
	int ret;
	int i;
	int tee_priv = 0;
	u32 reg_val = 0;
	u32 reg_offset = 0;
	struct amlkl_usage *pu;
	struct aml_mkl_dev *dev = filp->private_data;

	mutex_lock(&dev->lock);

	if (!param) {
		LOGE("Error: param data has Null\n");
		ret = KL_STATUS_ERROR_BAD_PARAM;
		goto exit;
	}

	pu = &param->usage;
	LOGI("kte:%d, levels:%d, kl_algo:%d, func_id:%d, mrk:%d\n",
		param->kt_handle, param->levels, param->kl_algo, param->func_id,
		param->mrk_cfg_index);
	LOGI("kt usage, crypto:%d, algo:%d, uid:%d\n", pu->crypto, pu->algo,
		pu->uid);

	if (pu->uid & ~KL_USERID_MASK ||
	    (pu->uid > AML_KT_USER_M2M_5 && pu->uid < AML_KT_USER_TSD) ||
	    (pu->uid > AML_KT_USER_TSE && pu->uid < KL_USERID_MASK) ||
	    pu->algo & ~KL_KEYALGO_MASK ||
	    (pu->algo > AML_KT_ALGO_DES && pu->algo < AML_KT_ALGO_NDL) ||
	    (pu->algo > AML_KT_ALGO_CSA2 && pu->algo < AML_KT_ALGO_HMAC) ||
	    (pu->algo > AML_KT_ALGO_HMAC && pu->algo < KL_KEYALGO_MASK) ||
	    pu->crypto & ~KL_FLAG_MASK || param->kl_algo > AML_KL_ALGO_AES) {
		LOGE("Error: param data has bad parameter\n");
		ret = KL_STATUS_ERROR_BAD_PARAM;
		goto exit;
	}

	/* 1. Read KL_REE_RDY to lock KL */
	if (aml_mkl_lock(dev) != KL_STATUS_OK) {
		LOGE("key ladder not ready\n");
		ret = KL_STATUS_ERROR_BAD_STATE;
		goto exit;
	}

	/* 2. Program Eks, fixed level to 3 */
	param->levels = AML_KL_LEVEL_3;
	for (i = 0; i < param->levels; i++) {
		ret = aml_mkl_program_key(dev->base_addr, dev->reg.ek_offset + i * 16,
					  param->eks[param->levels - 1 - i]);
		if (ret != 0) {
			LOGE("Error: Ek data has bad parameter\n");
			ret = KL_STATUS_ERROR_BAD_PARAM;
			goto exit;
		}
	}

	/* 3. Program KL_REE_CMD */
	reg_val = 0;
	reg_offset = dev->reg.cmd_offset;
	reg_val = (tee_priv << KL_TEE_PRIV_OFFSET | 0 << KL_LEVEL_OFFSET |
		   0 << KL_STAGE_OFFSET | 0 << KL_TEE_SEP_OFFSET);
	aml_mkl_iowrite32(reg_val, dev->base_addr, reg_offset);

	/* 4. Program KL_REE_CFG with KL_pending to 1 */
	reg_val = 0;
	reg_offset = dev->reg.cfg_offset;
	reg_val = (1 << KL_PENDING_OFFSET | param->kl_algo << KL_ALGO_OFFSET |
		   param->kl_mode << KL_MODE_OFFSET |
		   param->usage.crypto << KL_FLAG_OFFSET |
		   param->usage.algo << KL_KEYALGO_OFFSET |
		   param->usage.uid << KL_USERID_OFFSET |
		   (HANDLE_TO_KTE(param->kt_handle)) << KL_KTE_OFFSET |
		   param->mrk_cfg_index << KL_MRK_OFFSET |
		   (param->func_id << KL_FUNC_ID_OFFSET));
	aml_mkl_iowrite32(reg_val, dev->base_addr, reg_offset);

	/* 5. Poll KL_REE_CFG till KL_pending is 0 */
	ret = aml_mkl_read_pending(dev);
	if (ret == KL_STATUS_OK)
		LOGI("AML Key Ladder run success\n");

exit:
	mutex_unlock(&dev->lock);
	aml_mkl_unlock(dev);
	return ret;
}

static int aml_mkl_msr_run(struct file *filp, struct amlkl_params *param)
{
	int ret;
	int i;
	int tee_priv = 0;
	u32 reg_val = 0;
	u32 reg_offset = 0;
	struct amlkl_usage *pu;
	struct aml_mkl_dev *dev = filp->private_data;

	mutex_lock(&dev->lock);

	if (!param) {
		LOGE("Error: param data has Null\n");
		ret = KL_STATUS_ERROR_BAD_PARAM;
		goto exit;
	}

	pu = &param->usage;
	LOGI("kte:%d, kl_algo:%d, func_id:%d\n",
		param->kt_handle, param->kl_algo, param->func_id);
	LOGI("kt usage, crypto:%d, algo:%d, uid:%d\n", pu->crypto, pu->algo,
		pu->uid);

	if (pu->uid & ~KL_USERID_MASK ||
	    (pu->uid > AML_KT_USER_M2M_5 && pu->uid < AML_KT_USER_TSD) ||
	    (pu->uid > AML_KT_USER_TSE && pu->uid < KL_USERID_MASK) ||
	    pu->algo & ~KL_KEYALGO_MASK ||
	    (pu->algo > AML_KT_ALGO_DES && pu->algo < AML_KT_ALGO_NDL) ||
	    (pu->algo > AML_KT_ALGO_CSA2 && pu->algo < AML_KT_ALGO_HMAC) ||
	    (pu->algo > AML_KT_ALGO_HMAC && pu->algo < KL_KEYALGO_MASK) ||
	    pu->crypto & ~KL_FLAG_MASK ||
	    param->kl_algo > AML_KL_ALGO_AES) {
		LOGE("Error: param data has bad parameter\n");
		ret = KL_STATUS_ERROR_BAD_PARAM;
		goto exit;
	}

	if (param->func_id == MSR_KL_FUNC_ID_CWUK ||
			param->func_id == MSR_KL_FUNC_ID_SSUK ||
			param->func_id == MSR_KL_FUNC_ID_CAUK) {
		param->levels = MSR_KL_LEVEL_2;
	} else if (param->func_id == MSR_KL_FUNC_ID_CPUK ||
			param->func_id == MSR_KL_FUNC_ID_CCCK) {
		param->levels = MSR_KL_LEVEL_3;
	} else if (param->func_id == MSR_KL_FUNC_ID_TAUK) {
		param->levels = MSR_KL_LEVEL_1;
	} else {
		LOGE("Error: func_id data has bad parameter\n");
		ret = KL_STATUS_ERROR_BAD_PARAM;
		goto exit;
	}

	/* 1. Read KL_REE_RDY to lock KL */
	if (aml_mkl_lock(dev) != KL_STATUS_OK) {
		LOGE("key ladder not ready\n");
		ret = KL_STATUS_ERROR_BAD_STATE;
		goto exit;
	}

	/* 2. Program Eks */
	for (i = 0; i < param->levels; i++) {
		ret = aml_mkl_program_key(dev->base_addr, dev->reg.ek_offset + i * 16,
					  param->eks[param->levels - 1 - i]);
		if (ret != 0) {
			LOGE("Error: Ek data has bad parameter\n");
			ret = KL_STATUS_ERROR_BAD_PARAM;
			goto exit;
		}
	}

	/* 3. Program KL_REE_CMD */
	reg_val = 0;
	reg_offset = dev->reg.cmd_offset;
	reg_val = (param->func_id << KL_MSR_FUNCID_OFFSET |
			param->kl_algo << KL_MSR_KEYALGO_OFFSET |
			param->levels << KL_MSR_PAYLOAD_OFFSET);
	aml_mkl_iowrite32(reg_val, dev->base_addr, reg_offset);

	/* 4. Program KL_REE_CFG with KL_pending to 1 */
	reg_val = 0;
	reg_offset = dev->reg.cfg_offset;
	reg_val = (1 << KL_PENDING_OFFSET |
		   param->kl_mode << KL_MODE_OFFSET |
		   param->usage.crypto << KL_FLAG_OFFSET |
		   param->usage.algo << KL_KEYALGO_OFFSET |
		   param->usage.uid << KL_USERID_OFFSET |
		   (HANDLE_TO_KTE(param->kt_handle)) << KL_KTE_OFFSET |
		   (tee_priv << KL_TEE_PRIV_OFFSET) |
		   0 << KL_MSR_BUFLEVEL_OFFSET);
	aml_mkl_iowrite32(reg_val, dev->base_addr, reg_offset);

	/* 5. Poll KL_REE_CFG till KL_pending is 0 */
	ret = aml_mkl_read_pending(dev);
	if (ret == KL_STATUS_OK)
		LOGI("AML MSR Key Ladder run success\n");

exit:
	mutex_unlock(&dev->lock);
	aml_mkl_unlock(dev);
	return ret;
}

static long aml_mkl_ioctl(struct file *filp, unsigned int cmd,
			  unsigned long arg)
{
	int ret = -ENOTTY;
	struct amlkl_params kl_param;
	struct aml_mkl_dev *dev = filp->private_data;

	if (!dev->base_addr) {
		LOGE("ERROR: MKL iobase is zero\n");
		return -EFAULT;
	}

	switch (cmd) {
	case AML_MKL_IOCTL_RUN:
		memset(&kl_param, 0, sizeof(kl_param));
		if (copy_from_user(&kl_param, (uint32_t *)arg,
				   sizeof(kl_param))) {
			return -EFAULT;
		}

		if (kl_param.kl_mode == AML_KL_MODE_ETSI) {
			if (get_cpu_type() == MESON_CPU_MAJOR_ID_T5W)
				ret = aml_mkl_etsi_t5w_run(filp, &kl_param);
			else
				ret = aml_mkl_etsi_run(filp, &kl_param);
			if (ret != 0) {
				LOGE("MKL: aml_mkl_etsi_run failed retval=0x%08x\n",
					ret);
				return -EFAULT;
			}
		} else if (kl_param.kl_mode == AML_KL_MODE_AML) {
			if (get_cpu_type() == MESON_CPU_MAJOR_ID_T5W) {
				LOGE("MKL: t5w aml_mkl_run failed. not support.\n");
				return -EFAULT;
			}

			ret = aml_mkl_run(filp, &kl_param);
			if (ret != 0) {
				LOGE("MKL: aml_mkl_run failed retval=0x%08x\n",
				       ret);
				return -EFAULT;
			}
		} else if (kl_param.kl_mode == AML_KL_MODE_MSR) {
			if (get_cpu_type() == MESON_CPU_MAJOR_ID_T5W) {
				LOGE("MKL: t5w aml_mkl_msr_run failed. not support.\n");
				return -EFAULT;
			}

			ret = aml_mkl_msr_run(filp, &kl_param);
			if (ret != 0) {
				LOGE("MKL: aml_mkl_msr_run failed retval=0x%08x\n",
				       ret);
				return -EFAULT;
			}
		}
		break;
	default:
		LOGE("No appropriate IOCTL found\n");
	}

	return ret;
}

static const struct file_operations aml_mkl_fops = {
	.owner = THIS_MODULE,
	.open = aml_mkl_open,
	.release = aml_mkl_release,
	.unlocked_ioctl = aml_mkl_ioctl,
	.compat_ioctl = aml_mkl_ioctl,
};

int aml_mkl_init(struct class *aml_mkl_class, struct platform_device *pdev)
{
	int ret = -1;
	struct device *device;
	struct resource *res;

	mutex_init(&aml_mkl_dev.lock);
	if (alloc_chrdev_region(&aml_mkl_devt, 0, DEVICE_INSTANCES,
				DEVICE_NAME) < 0) {
		LOGE("%s device can't be allocated.\n", DEVICE_NAME);
		return -ENXIO;
	}

	cdev_init(&aml_mkl_dev.cdev, &aml_mkl_fops);
	aml_mkl_dev.cdev.owner = THIS_MODULE;
	ret = cdev_add(&aml_mkl_dev.cdev,
		       MKDEV(MAJOR(aml_mkl_devt), MINOR(aml_mkl_devt)), 1);
	aml_mkl_dev.base_addr = NULL;
	if (unlikely(ret < 0))
		goto cdev_add_error;

	device = device_create(aml_mkl_class, NULL, aml_mkl_devt, NULL,
			       DEVICE_NAME);
	if (IS_ERR(device)) {
		LOGE("device_create failed\n");
		ret = PTR_ERR(device);
		goto device_create_error;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		LOGE("%s: platform_get_resource is failed\n", __func__);
		return -ENOMEM;
	}

	aml_mkl_dev.base_addr = devm_ioremap_resource(&pdev->dev, res);
	if (!aml_mkl_dev.base_addr) {
		LOGE("%s base addr error\n", __func__);
		return -ENOMEM;
	}

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_T5W) {
		aml_mkl_dev.t5w_reg.start0_offset = 0x80;
		aml_mkl_dev.t5w_reg.key1_offset = 0x100;
		aml_mkl_dev.t5w_reg.nonce_offset = aml_mkl_dev.t5w_reg.key1_offset + 16;
		aml_mkl_dev.t5w_reg.key2_offset = aml_mkl_dev.t5w_reg.nonce_offset + 16;
		aml_mkl_dev.t5w_reg.key3_offset = aml_mkl_dev.t5w_reg.key2_offset + 16;
		aml_mkl_dev.t5w_reg.key4_offset = aml_mkl_dev.t5w_reg.key3_offset + 16;
		aml_mkl_dev.t5w_reg.key5_offset = aml_mkl_dev.t5w_reg.key4_offset + 16;
		aml_mkl_dev.t5w_reg.key6_offset = aml_mkl_dev.t5w_reg.key5_offset + 16;
		aml_mkl_dev.t5w_reg.key7_offset = aml_mkl_dev.t5w_reg.key6_offset + 16;
	} else {
		aml_mkl_dev.reg.rdy_offset = 0x80;
		aml_mkl_dev.reg.cfg_offset = 0x88;
		aml_mkl_dev.reg.cmd_offset = 0x8c;
		aml_mkl_dev.reg.ek_offset = 0x90;
	}

	return 0;

device_create_error:
	mutex_destroy(&aml_mkl_dev.lock);
	cdev_del(&aml_mkl_dev.cdev);
cdev_add_error:
	mutex_destroy(&aml_mkl_dev.lock);
	unregister_chrdev_region(aml_mkl_devt, DEVICE_INSTANCES);
	return ret;
}

void aml_mkl_exit(struct class *aml_mkl_class, struct platform_device *pdev)
{
	device_destroy(aml_mkl_class, aml_mkl_devt);
	cdev_del(&aml_mkl_dev.cdev);
	unregister_chrdev_region(MKDEV(MAJOR(aml_mkl_devt),
				       MINOR(aml_mkl_devt)),
				 DEVICE_INSTANCES);
	mutex_destroy(&aml_mkl_dev.lock);
	LOGI("%s module has been unloaded.\n", DEVICE_NAME);
}

static int aml_mkl_probe(struct platform_device *pdev)
{
	int ret = 0;

	aml_mkl_class = class_create(THIS_MODULE, "aml_mkl");
	if (IS_ERR(aml_mkl_class)) {
		LOGE("class_create failed\n");
		ret = PTR_ERR(aml_mkl_class);
		return ret;
	}

	ret = aml_mkl_init(aml_mkl_class, pdev);
	if (unlikely(ret < 0)) {
		LOGE("aml_mkl_core_init failed\n");
		goto device_create_error;
	}

	LOGI("%s module has been loaded.\n", KBUILD_MODNAME);
	return 0;

device_create_error:
	class_destroy(aml_mkl_class);
	return ret;
}

static int aml_mkl_remove(struct platform_device *pdev)
{
	aml_mkl_exit(aml_mkl_class, pdev);
	class_destroy(aml_mkl_class);
	LOGI("%s module has been unloaded.\n", KBUILD_MODNAME);
	return 0;
}

static const struct of_device_id aml_mkl_dt_ids[] = {
	{
		.compatible = "amlogic,aml_mkl",
	},
	{}
};

static struct platform_driver aml_mkl_drv = {
	.probe = aml_mkl_probe,
	.remove = aml_mkl_remove,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = aml_mkl_dt_ids,
		.owner = THIS_MODULE,
	},
};

module_platform_driver(aml_mkl_drv);
MODULE_DEVICE_TABLE(of, aml_mkl_dt_ids);
MODULE_AUTHOR("Amlogic Inc.");
MODULE_DESCRIPTION("AML MKL device driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(MODULES_MKL_VERSION_BASE);
