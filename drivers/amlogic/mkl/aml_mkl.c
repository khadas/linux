// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/aml_mkl.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/cdev.h>
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
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/aml_kt.h>
#include "aml_mkl_log.h"

#define AML_MKL_DEVICE_NAME "aml_mkl"
#define DEVICE_INSTANCES 1

/* kl type */
#define KL_TYPE_OLD (0)
#define KL_TYPE_NEW (1)

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
	u32 kl_type;
};

static dev_t aml_mkl_devt;
static struct aml_mkl_dev aml_mkl_dev;
static struct class *aml_mkl_class;
static struct dentry *aml_mkl_debug_dent;
int kl_log_level = 3;

static int aml_mkl_init_dbgfs(void)
{
	if (!aml_mkl_debug_dent) {
		aml_mkl_debug_dent = debugfs_create_dir("aml_mkl", NULL);
		if (!aml_mkl_debug_dent) {
			LOGE("can not create debugfs directory\n");
			return -ENOMEM;
		}

		debugfs_create_u32("log_level", 0644, aml_mkl_debug_dent, &kl_log_level);
	}
	return 0;
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
		iowrite32(data32[i], (char *)base_addr + offset + i * sizeof(u32));

	return 0;
}

static int aml_mkl_lock(struct aml_mkl_dev *dev)
{
	int cnt = 0;
	int ret = KL_STATUS_OK;

	mutex_lock(&dev->lock);
	if (dev->kl_type == KL_TYPE_OLD) {
		while ((ioread32((char *)dev->base_addr + dev->t5w_reg.start0_offset) >> 31) & 1) {
			if (cnt++ > KL_PENDING_WAIT_TIMEOUT) {
				LOGE("Error: wait KL ready timeout\n");
				ret = KL_STATUS_ERROR_TIMEOUT;
			}
		}
	} else {
		while (ioread32((char *)dev->base_addr + dev->reg.rdy_offset) != 1) {
			if (cnt++ > KL_PENDING_WAIT_TIMEOUT) {
				LOGE("Error: wait KL ready timeout\n");
				ret = KL_STATUS_ERROR_TIMEOUT;
			}
		}
	}

	mutex_unlock(&dev->lock);
	return ret;
}

static void aml_mkl_unlock(struct aml_mkl_dev *dev)
{
	mutex_lock(&dev->lock);
	iowrite32(1, (char *)dev->base_addr + dev->reg.rdy_offset);
	mutex_unlock(&dev->lock);
}

static int aml_mkl_read_pending(struct aml_mkl_dev *dev)
{
	int ret = -1;
	int cnt = 0;
	u32 reg_ret = 0;

	do {
		reg_ret = ioread32((char *)dev->base_addr + dev->reg.cfg_offset);
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
	int ret = KL_STATUS_OK;
	int i;
	int tee_priv = 0;
	u32 reg_val = 0;
	u32 reg_offset = 0;
	struct amlkl_usage *pu;
	struct aml_mkl_dev *dev = filp->private_data;

	if (!param) {
		LOGE("Error: param data has Null\n");
		ret = KL_STATUS_ERROR_BAD_PARAM;
		goto exit;
	}

	pu = &param->usage;
	LOGD("kte:%d, levels:%d, mid:%#x, kl_algo:%d, mrk:%d\n",
		param->kt_handle, param->levels, param->module_id,
		param->kl_algo, param->mrk_cfg_index);
	LOGD("kt usage, crypto:%d, algo:%d, uid:%d\n", pu->crypto, pu->algo,
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
	iowrite32(reg_val, (char *)dev->base_addr + reg_offset);

	/* 4. Program KL_REE_CFG with KL_pending to 1 */
	reg_val = 0;
	reg_offset = dev->reg.cfg_offset;
	reg_val = (1 << KL_PENDING_OFFSET | param->kl_algo << KL_ALGO_OFFSET |
		   param->kl_mode << KL_MODE_OFFSET |
		   param->usage.crypto << KL_FLAG_OFFSET |
		   param->usage.algo << KL_KEYALGO_OFFSET |
		   param->usage.uid << KL_USERID_OFFSET |
		   param->kt_handle << KL_KTE_OFFSET |
		   param->mrk_cfg_index << KL_MRK_OFFSET | param->func_id);
	iowrite32(reg_val, (char *)dev->base_addr + reg_offset);

	/* 5. Poll KL_REE_CFG till KL_pending is 0 */
	ret = aml_mkl_read_pending(dev);
	if (ret == KL_STATUS_OK)
		LOGI("ETSI Key Ladder run success\n");

exit:
	aml_mkl_unlock(dev);
	return ret;
}

static int aml_mkl_etsi_t5w_run(struct file *filp, struct amlkl_params *param)
{
	int ret = KL_STATUS_OK;
	int i;
	u32 reg_val = 0;
	u32 reg_offset = 0;
	u32 key_addrs[7] = {0};
	u8 zero[16] = {0};
	struct aml_mkl_dev *dev = filp->private_data;

	if (!param) {
		LOGE("Error: param data has Null\n");
		return KL_STATUS_ERROR_BAD_PARAM;
	}

	LOGD("kte:%d, levels:%d, kl_num:%d\n", param->kt_handle, param->levels, param->kl_num);

	if (param->levels < AML_KL_LEVEL_3 ||
		param->levels > AML_KL_LEVEL_6 ||
	    param->kl_num > 10) {
		LOGE("Error: param data has bad parameter\n");
		return KL_STATUS_ERROR_BAD_PARAM;
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
		return KL_STATUS_ERROR_BAD_PARAM;
	}

	/* 2. Program KL_REE_CFG */
	reg_val = 0;
	reg_offset = dev->t5w_reg.start0_offset;
	reg_val = (param->kl_num << 24 | 0 << 22 |
		   param->kt_handle << 16 |
		   param->reserved[1] << 8 | param->reserved[0]);
	iowrite32(reg_val, (char *)dev->base_addr + reg_offset);

	/* 3. Wait Busy Done */
	if (aml_mkl_lock(dev) != KL_STATUS_OK) {
		LOGE("key ladder is busy\n");
		ret = KL_STATUS_ERROR_BAD_STATE;
		goto exit;
	}

	LOGI("ETSI T5W Key Ladder run success\n");

exit:
	aml_mkl_unlock(dev);

	return ret;
}

static int aml_mkl_run(struct file *filp, struct amlkl_params *param)
{
	int ret = KL_STATUS_OK;
	int i;
	int tee_priv = 0;
	u32 reg_val = 0;
	u32 reg_offset = 0;
	struct amlkl_usage *pu;
	struct aml_mkl_dev *dev = filp->private_data;

	if (!param) {
		LOGE("Error: param data has Null\n");
		return KL_STATUS_ERROR_BAD_PARAM;
	}

	pu = &param->usage;
	LOGD("kte:%d, levels:%d, kl_algo:%d, func_id:%d, mrk:%d\n",
		param->kt_handle, param->levels, param->kl_algo, param->func_id,
		param->mrk_cfg_index);
	LOGD("kt usage, crypto:%d, algo:%d, uid:%d\n", pu->crypto, pu->algo,
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
		return KL_STATUS_ERROR_BAD_PARAM;
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
	iowrite32(reg_val, (char *)dev->base_addr + reg_offset);

	/* 4. Program KL_REE_CFG with KL_pending to 1 */
	reg_val = 0;
	reg_offset = dev->reg.cfg_offset;
	reg_val = (1 << KL_PENDING_OFFSET | param->kl_algo << KL_ALGO_OFFSET |
		   param->kl_mode << KL_MODE_OFFSET |
		   param->usage.crypto << KL_FLAG_OFFSET |
		   param->usage.algo << KL_KEYALGO_OFFSET |
		   param->usage.uid << KL_USERID_OFFSET |
		   param->kt_handle << KL_KTE_OFFSET |
		   param->mrk_cfg_index << KL_MRK_OFFSET |
		   (param->func_id << KL_FUNC_ID_OFFSET));
	iowrite32(reg_val, (char *)dev->base_addr + reg_offset);

	/* 5. Poll KL_REE_CFG till KL_pending is 0 */
	ret = aml_mkl_read_pending(dev);
	if (ret == KL_STATUS_OK)
		LOGI("AML Key Ladder run success\n");

exit:
	aml_mkl_unlock(dev);

	return ret;
}

static int aml_mkl_msr_run(struct file *filp, struct amlkl_params *param)
{
	int ret = KL_STATUS_OK;
	int i;
	int tee_priv = 0;
	u32 reg_val = 0;
	u32 reg_offset = 0;
	struct amlkl_usage *pu;
	struct aml_mkl_dev *dev = filp->private_data;

	if (!param) {
		LOGE("Error: param data has Null\n");
		return KL_STATUS_ERROR_BAD_PARAM;
	}

	pu = &param->usage;
	LOGD("kte:%d, kl_algo:%d, func_id:%d\n",
		param->kt_handle, param->kl_algo, param->func_id);
	LOGD("kt usage, crypto:%d, algo:%d, uid:%d\n", pu->crypto, pu->algo,
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
		return KL_STATUS_ERROR_BAD_PARAM;
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
		return KL_STATUS_ERROR_BAD_PARAM;
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
	iowrite32(reg_val, (char *)dev->base_addr + reg_offset);

	/* 4. Program KL_REE_CFG with KL_pending to 1 */
	reg_val = 0;
	reg_offset = dev->reg.cfg_offset;
	reg_val = (1 << KL_PENDING_OFFSET |
		   param->kl_mode << KL_MODE_OFFSET |
		   param->usage.crypto << KL_FLAG_OFFSET |
		   param->usage.algo << KL_KEYALGO_OFFSET |
		   param->usage.uid << KL_USERID_OFFSET |
		   param->kt_handle << KL_KTE_OFFSET |
		   (tee_priv << KL_TEE_PRIV_OFFSET) |
		   0 << KL_MSR_BUFLEVEL_OFFSET);
	iowrite32(reg_val, (char *)dev->base_addr + reg_offset);

	/* 5. Poll KL_REE_CFG till KL_pending is 0 */
	ret = aml_mkl_read_pending(dev);
	if (ret == KL_STATUS_OK)
		LOGI("AML MSR Key Ladder run success\n");

exit:
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
			if (dev->kl_type == KL_TYPE_OLD)
				ret = aml_mkl_etsi_t5w_run(filp, &kl_param);
			else
				ret = aml_mkl_etsi_run(filp, &kl_param);
			if (ret != 0) {
				LOGE("MKL: aml_mkl_etsi_run failed retval=0x%08x\n",
					ret);
				return -EFAULT;
			}
		} else if (kl_param.kl_mode == AML_KL_MODE_AML) {
			if (dev->kl_type == KL_TYPE_OLD) {
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
			if (dev->kl_type == KL_TYPE_OLD) {
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

static int aml_mkl_get_dts_info(struct aml_mkl_dev *dev, struct platform_device *pdev)
{
	int ret = 0;
	u32 t5w_offset[2];
	u32 offset[4];

	if (unlikely(!dev)) {
		LOGE("Empty aml_mkl_dev\n");
		return -1;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "kl_type", &dev->kl_type);
	if (ret) {
		LOGE("%s: not found 0x%x\n", "kl_type", dev->kl_type);
		return -1;
	}

	/* kl register offset */
	if (dev->kl_type == KL_TYPE_OLD) {
		if (of_property_read_u32_array(pdev->dev.of_node, "kl_offset", t5w_offset,
			ARRAY_SIZE(t5w_offset)) == 0) {
			dev->t5w_reg.start0_offset = t5w_offset[0];
			dev->t5w_reg.key1_offset = t5w_offset[1];
			dev->t5w_reg.nonce_offset = dev->t5w_reg.key1_offset + 16;
			dev->t5w_reg.key2_offset = dev->t5w_reg.nonce_offset + 16;
			dev->t5w_reg.key3_offset = dev->t5w_reg.key2_offset + 16;
			dev->t5w_reg.key4_offset = dev->t5w_reg.key3_offset + 16;
			dev->t5w_reg.key5_offset = dev->t5w_reg.key4_offset + 16;
			dev->t5w_reg.key6_offset = dev->t5w_reg.key5_offset + 16;
			dev->t5w_reg.key7_offset = dev->t5w_reg.key6_offset + 16;
		} else {
			LOGE("%s: T5W not found\n", "kl_offset");
			return -1;
		}
	} else {
		if (of_property_read_u32_array(pdev->dev.of_node, "kl_offset", offset,
			ARRAY_SIZE(offset)) == 0) {
			dev->reg.rdy_offset = offset[0];
			dev->reg.cfg_offset = offset[1];
			dev->reg.cmd_offset = offset[2];
			dev->reg.ek_offset = offset[3];
		} else {
			LOGE("%s: not found\n", "kl_offset");
			return -1;
		}
	}

	return ret;
}

int aml_mkl_init(struct class *aml_mkl_class, struct platform_device *pdev)
{
	int ret = -1;
	struct device *device;
	struct resource *res;

	if (alloc_chrdev_region(&aml_mkl_devt, 0, DEVICE_INSTANCES,
				AML_MKL_DEVICE_NAME) < 0) {
		LOGE("%s device can't be allocated.\n", AML_MKL_DEVICE_NAME);
		return -ENXIO;
	}

	cdev_init(&aml_mkl_dev.cdev, &aml_mkl_fops);
	aml_mkl_dev.cdev.owner = THIS_MODULE;
	ret = cdev_add(&aml_mkl_dev.cdev,
		       MKDEV(MAJOR(aml_mkl_devt), MINOR(aml_mkl_devt)), 1);
	aml_mkl_dev.base_addr = NULL;
	if (unlikely(ret < 0))
		goto unregister_chrdev;

	device = device_create(aml_mkl_class, NULL, aml_mkl_devt, NULL,
			       AML_MKL_DEVICE_NAME);
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

	aml_mkl_dev.base_addr = devm_ioremap_resource(&pdev->dev, res);
	if (!aml_mkl_dev.base_addr) {
		LOGE("%s base addr error\n", __func__);
		ret = -ENOMEM;
		goto destroy_device;
	}

	ret = aml_mkl_get_dts_info(&aml_mkl_dev, pdev);
	if (ret != 0) {
		LOGE("%s: cannot find match dts info\n", __func__);
		ret = -EINVAL;
		goto destroy_device;
	}

	mutex_init(&aml_mkl_dev.lock);
	return ret;

destroy_device:
	device_destroy(aml_mkl_class, aml_mkl_devt);
delete_cdev:
	cdev_del(&aml_mkl_dev.cdev);
unregister_chrdev:
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
}

static int aml_mkl_probe(struct platform_device *pdev)
{
	int ret = 0;

	aml_mkl_class = class_create(THIS_MODULE, AML_MKL_DEVICE_NAME);
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

	ret = aml_mkl_init_dbgfs();
	if (ret) {
		LOGE("aml_mkl_init_dbgfs failed\n");
		goto device_create_error;
	}

	return 0;

device_create_error:
	class_destroy(aml_mkl_class);
	return ret;
}

static int aml_mkl_remove(struct platform_device *pdev)
{
	aml_mkl_exit(aml_mkl_class, pdev);
	class_destroy(aml_mkl_class);
	debugfs_remove_recursive(aml_mkl_debug_dent);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id aml_mkl_dt_match[] = {
	{
		.compatible = "amlogic,aml_mkl",
	},
	{}
};
MODULE_DEVICE_TABLE(of, aml_mkl_dt_match);
#else
#define aml_mkl_dt_match NULL
#endif

static struct platform_driver aml_mkl_drv = {
	.probe = aml_mkl_probe,
	.remove = aml_mkl_remove,
	.driver = {
		.name = AML_MKL_DEVICE_NAME,
		.of_match_table = aml_mkl_dt_match,
		.owner = THIS_MODULE,
	},
};

module_platform_driver(aml_mkl_drv);
MODULE_DEVICE_TABLE(of, aml_mkl_dt_match);
MODULE_AUTHOR("Amlogic Inc.");
MODULE_DESCRIPTION("AML MKL device driver");
MODULE_LICENSE("GPL v2");
