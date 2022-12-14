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

#define DEVICE_NAME "aml_mkl"
#define DEVICE_INSTANCES 1

static DEFINE_MUTEX(aml_mkl_mutex);

/* kl register */
#define MKL_REE_RDY (0x0020 << 2)
#define MKL_REE_DEBUG (0x0021 << 2)
#define MKL_REE_CFG (0x0022 << 2)
#define MKL_REE_CMD (0x0023 << 2)
#define MKL_REE_EK (0x0024 << 2)

/* kl offset */
#define KL_PENDING_OFFSET (31)
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
};

static dev_t aml_mkl_devt;
static struct aml_mkl_dev aml_mkl_dev_s;
static void *p_mkl_hw_base;
static struct class *aml_mkl_class;

__attribute__((always_inline)) static inline u32
aml_mkl_ioread32(void *addr)
{
	u32 val;

	val = readl(addr);

	return val;
}

__attribute__((always_inline)) static inline void
aml_mkl_iowrite32(u32 val, void *addr)
{
	writel(val, addr);
}

static int aml_mkl_open(struct inode *inode, struct file *file)
{
	int ret = -1;
	struct aml_mkl_dev *dev;

	dev = container_of(inode->i_cdev, struct aml_mkl_dev, cdev);
	file->private_data = dev;

	ret = 0;
	return ret;
}

static int aml_mkl_release(struct inode *inode, struct file *file)
{
	int ret = -1;
	__maybe_unused struct aml_mkl_dev *dev =
		container_of(inode->i_cdev, struct aml_mkl_dev, cdev);

	if (!file->private_data)
		return 0;

	file->private_data = NULL;

	ret = 0;
	return ret;
}

static int aml_mkl_program_key(void *iobase, const uint8_t *data, uint32_t len)
{
	int i = 0;
	const u32 *data32 = (const u32 *)data;

	if (!data32 || len != 16)
		return -1;

	for (i = 0; i < 4; i++)
		aml_mkl_iowrite32(data32[i],
				  (iobase + MKL_REE_EK) + i * sizeof(uint32_t));

	return 0;
}

static int aml_mkl_lock(void *iobase)
{
	int cnt = 0;

	while (aml_mkl_ioread32(iobase + MKL_REE_RDY) != 1) {
		if (cnt++ > KL_PENDING_WAIT_TIMEOUT) {
			pr_err("Error: wait KT ready timeout\n");
			return KL_STATUS_ERROR_TIMEOUT;
		}
	}

	return KL_STATUS_OK;
}

static void aml_mkl_unlock(void *iobase)
{
	aml_mkl_iowrite32(1, iobase + MKL_REE_RDY);
}

static int aml_mkl_etsi_run(void *iobase, struct amlkl_params *param)
{
	int ret;
	int i;
	int tee_priv = 0;
	u32 reg_val = 0;
	void *reg_addr;
	struct amlkl_usage *pu;

	mutex_lock(&aml_mkl_mutex);

	if (!param) {
		ret = KL_STATUS_ERROR_BAD_PARAM;
		goto exit;
	}

	pu = &param->usage;
	pr_info("kth:%d, levels:%d, mid:%#x, kl_algo:%d, mrk:%d\n",
		param->kt_handle, param->levels, param->module_id,
		param->kl_algo, param->mrk_cfg_index);
	pr_info("kt usage, crypto:%d, algo:%d, uid:%d\n", pu->crypto, pu->algo,
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
	if (aml_mkl_lock(iobase) != KL_STATUS_OK) {
		pr_err("key ladder not ready\n");
		ret = KL_STATUS_ERROR_BAD_STATE;
		goto exit;
	}

	/* 2. Program Eks */
	for (i = 0; i < param->levels; i++) {
		ret = aml_mkl_program_key(iobase + i * 16, param->eks[param->levels - 1 - i],
						sizeof(param->eks[param->levels - 1 - i]));
		if (ret != 0) {
			pr_err("Error: Ek data has bad parameter\n");
			ret = KL_STATUS_ERROR_BAD_PARAM;
			goto exit;
		}
	}

	/* 3. Program KL_REE_MID */
	reg_val = 0;
	reg_addr = (void *)(iobase + MKL_REE_CMD);
	reg_val = (param->module_id << KL_MID_OFFSET |
		   KL_ETSI_MID_MSG << KL_MID_EXTRA_OFFSET |
		   tee_priv << KL_TEE_PRIV_OFFSET);
	aml_mkl_iowrite32(reg_val, reg_addr);

	/* 4. Program KL_REE_CFG with KL_pending to 1 */
	reg_val = 0;
	reg_addr = (void *)(iobase + MKL_REE_CFG);
	reg_val = (1 << KL_PENDING_OFFSET | param->kl_algo << KL_ALGO_OFFSET |
		   param->kl_mode << KL_MODE_OFFSET |
		   param->usage.crypto << KL_FLAG_OFFSET |
		   param->usage.algo << KL_KEYALGO_OFFSET |
		   param->usage.uid << KL_USERID_OFFSET |
		   (HANDLE_TO_KTE(param->kt_handle)) << KL_KTE_OFFSET |
		   param->mrk_cfg_index << KL_MRK_OFFSET | param->func_id);
	aml_mkl_iowrite32(reg_val, reg_addr);

	/* 5. Poll KL_REE_CFG till KL_pending is 0 */
	while ((ret = aml_mkl_ioread32(iobase + MKL_REE_CFG) &
		      (1 << KL_PENDING_OFFSET)))
		;

	/* 6.	Write KL_REE_RDY to release KL */
	aml_mkl_unlock(iobase);

	/* 7. Get final status */
	ret = (ret >> KL_STATUS_OFFSET) & KL_STATUS_MASK;

	if (ret != KL_STATUS_OK) {
		pr_err("Error code: %d\n", ret);
		ret = KL_STATUS_ERROR_BAD_STATE;
		goto exit;
	}

	pr_info("etsi key ladder run success\n");

exit:
	mutex_unlock(&aml_mkl_mutex);
	return ret;
}

static int aml_mkl_run(void *iobase, struct amlkl_params *param)
{
	int ret;
	int i;
	int tee_priv = 0;
	u32 reg_val = 0;
	void *reg_addr;
	struct amlkl_usage *pu;

	mutex_lock(&aml_mkl_mutex);

	if (!param) {
		ret = KL_STATUS_ERROR_BAD_PARAM;
		goto exit;
	}

	pu = &param->usage;
	pr_info("kth:%d, levels:%d, kl_algo:%d, func_id:%d, mrk:%d\n",
		param->kt_handle, param->levels, param->kl_algo, param->func_id,
		param->mrk_cfg_index);
	pr_info("kt usage, crypto:%d, algo:%d, uid:%d\n", pu->crypto, pu->algo,
		pu->uid);

	if (pu->uid & ~KL_USERID_MASK ||
	    (pu->uid > AML_KT_USER_M2M_5 && pu->uid < AML_KT_USER_TSD) ||
	    (pu->uid > AML_KT_USER_TSE && pu->uid < KL_USERID_MASK) ||
	    pu->algo & ~KL_KEYALGO_MASK ||
	    (pu->algo > AML_KT_ALGO_DES && pu->algo < AML_KT_ALGO_NDL) ||
	    (pu->algo > AML_KT_ALGO_CSA2 && pu->algo < AML_KT_ALGO_HMAC) ||
	    (pu->algo > AML_KT_ALGO_HMAC && pu->algo < KL_KEYALGO_MASK) ||
	    pu->crypto & ~KL_FLAG_MASK || param->levels != AML_KL_LEVEL_3 ||
	    param->kl_algo > AML_KL_ALGO_AES) {
		ret = KL_STATUS_ERROR_BAD_PARAM;
		goto exit;
	}

	/* 1. Read KL_REE_RDY to lock KL */
	if (aml_mkl_lock(iobase) != KL_STATUS_OK) {
		pr_err("key ladder not ready\n");
		ret = KL_STATUS_ERROR_BAD_STATE;
		goto exit;
	}

	/* 2. Program Eks */
	for (i = 0; i < param->levels; i++) {
		ret = aml_mkl_program_key(iobase + i * 16, param->eks[param->levels - 1 - i],
						sizeof(param->eks[param->levels - 1 - i]));
		if (ret != 0) {
			pr_err("Error: Ek data has bad parameter\n");
			ret = KL_STATUS_ERROR_BAD_PARAM;
			goto exit;
		}
	}

	/* 3. Program KL_REE_CMD */
	reg_val = 0;
	reg_addr = (void *)(iobase + MKL_REE_CMD);
	reg_val = (tee_priv << KL_TEE_PRIV_OFFSET | 0 << KL_LEVEL_OFFSET |
		   0 << KL_STAGE_OFFSET | 0 << KL_TEE_SEP_OFFSET);
	aml_mkl_iowrite32(reg_val, reg_addr);

	/* 4. Program KL_REE_CFG with KL_pending to 1 */
	reg_val = 0;
	reg_addr = (void *)(iobase + MKL_REE_CFG);
	reg_val = (1 << KL_PENDING_OFFSET | param->kl_algo << KL_ALGO_OFFSET |
		   param->kl_mode << KL_MODE_OFFSET |
		   param->usage.crypto << KL_FLAG_OFFSET |
		   param->usage.algo << KL_KEYALGO_OFFSET |
		   param->usage.uid << KL_USERID_OFFSET |
		   (HANDLE_TO_KTE(param->kt_handle)) << KL_KTE_OFFSET |
		   param->mrk_cfg_index << KL_MRK_OFFSET |
		   (param->func_id << KL_FUNC_ID_OFFSET));
	aml_mkl_iowrite32(reg_val, reg_addr);

	/* 5. Poll KL_REE_CFG till KL_pending is 0 */
	while ((ret = aml_mkl_ioread32(iobase + MKL_REE_CFG) &
		      (1 << KL_PENDING_OFFSET)))
		;

	/* 6.	Write KL_REE_RDY to release KL */
	aml_mkl_unlock(iobase);

	/* 7. Get final status */
	ret = (ret >> KL_STATUS_OFFSET) & KL_STATUS_MASK;
	switch (ret) {
	case 0:
		break;
	case 1:
		pr_err("Permission Denied Error code: %d\n", ret);
		ret = KL_STATUS_ERROR_BAD_STATE;
		goto exit;
	case 2:
	case 3:
	default:
		pr_err("OTP or KL Error code: %d\n", ret);
		ret = KL_STATUS_ERROR_OTP;
		goto exit;
	}

	pr_info("aml key ladder run success\n");

exit:
	mutex_unlock(&aml_mkl_mutex);
	return ret;
}

static long aml_mkl_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	struct amlkl_params kl_param;
	int ret = -ENOTTY;

	if (!p_mkl_hw_base) {
		pr_err("ERROR: MKL iobase is zero\n");
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
			ret = aml_mkl_etsi_run((void *)p_mkl_hw_base,
					       &kl_param);
			if (ret != 0) {
				pr_err("MKL: aml_mkl_etsi_run failed retval=0x%08x\n",
				       ret);
				return -EFAULT;
			}
		} else if (kl_param.kl_mode == AML_KL_MODE_AML) {
			ret = aml_mkl_run((void *)p_mkl_hw_base, &kl_param);
			if (ret != 0) {
				pr_err("MKL: aml_mkl_run failed retval=0x%08x\n",
				       ret);
				return -EFAULT;
			}
		}
		break;
	default:
		pr_err("No appropriate IOCTL found\n");
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

	if (alloc_chrdev_region(&aml_mkl_devt, 0, DEVICE_INSTANCES,
				DEVICE_NAME) < 0) {
		pr_err("%s device can't be allocated.\n", DEVICE_NAME);
		return -ENXIO;
	}

	cdev_init(&aml_mkl_dev_s.cdev, &aml_mkl_fops);
	aml_mkl_dev_s.cdev.owner = THIS_MODULE;
	ret = cdev_add(&aml_mkl_dev_s.cdev,
		       MKDEV(MAJOR(aml_mkl_devt), MINOR(aml_mkl_devt)), 1);
	p_mkl_hw_base = NULL;
	if (unlikely(ret < 0))
		goto cdev_add_error;

	device = device_create(aml_mkl_class, NULL, aml_mkl_devt, NULL,
			       DEVICE_NAME);
	if (IS_ERR(device)) {
		pr_err("device_create failed\n");
		ret = PTR_ERR(device);
		goto device_create_error;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err("%s: platform_get_resource is failed\n", __func__);
		return -ENOMEM;
	}

	p_mkl_hw_base = devm_ioremap_nocache(&pdev->dev, res->start,
					     resource_size(res));
	if (!p_mkl_hw_base) {
		pr_err("%s base addr error\n", __func__);
		return -ENOMEM;
	}
	pr_info("%s module's been loaded. addr:0x%x\n", DEVICE_NAME,
		(unsigned int)res->start);
	return 0;

device_create_error:
	cdev_del(&aml_mkl_dev_s.cdev);
cdev_add_error:
	unregister_chrdev_region(aml_mkl_devt, DEVICE_INSTANCES);
	return ret;
}

void aml_mkl_exit(struct class *aml_mkl_class, struct platform_device *pdev)
{
	device_destroy(aml_mkl_class, aml_mkl_devt);
	cdev_del(&aml_mkl_dev_s.cdev);
	unregister_chrdev_region(MKDEV(MAJOR(aml_mkl_devt),
				       MINOR(aml_mkl_devt)),
				 DEVICE_INSTANCES);
	pr_info("%s module's been unloaded.\n", DEVICE_NAME);
}

static int aml_mkl_probe(struct platform_device *pdev)
{
	int ret = -1;

	aml_mkl_class = class_create(THIS_MODULE, "aml_mkl");
	if (IS_ERR(aml_mkl_class)) {
		pr_err("class_create failed\n");
		ret = PTR_ERR(aml_mkl_class);
		return ret;
	}

	ret = aml_mkl_init(aml_mkl_class, pdev);
	if (unlikely(ret < 0)) {
		pr_err("aml_mkl_core_init failed\n");
		goto device_create_error;
	}

	pr_info("%s module's been loaded.\n", KBUILD_MODNAME);
	return 0;

device_create_error:
	class_destroy(aml_mkl_class);
	return ret;
}

static int aml_mkl_remove(struct platform_device *pdev)
{
	aml_mkl_exit(aml_mkl_class, pdev);

	class_destroy(aml_mkl_class);
	pr_info("%s module's been unloaded.\n", KBUILD_MODNAME);
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
