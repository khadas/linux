// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/aml_key.h>
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
#include "../aml_kt_log.h"
#include "../aml_kt_dev.h"

#define AML_KEY_DEVICE_NAME "key"
#define DEVICE_INSTANCES 1

#define KT_SUCCESS (0)
#define KT_ERROR (-1)

struct aml_key_dev {
	struct cdev cdev;
};

static struct aml_key_dev aml_key_dev;
static dev_t aml_key_devt;
static struct class *aml_key_class;
static struct dentry *aml_key_debug_dent;
u32 old_kt_log_level = 3;

static int aml_key_init_dbgfs(void)
{
	if (!aml_key_debug_dent) {
		aml_key_debug_dent = debugfs_create_dir("aml_key", NULL);
		if (!aml_key_debug_dent) {
			KT_LOGE("can not create debugfs directory\n");
			return -ENOMEM;
		}

		debugfs_create_u32("log_level", 0644, aml_key_debug_dent, &old_kt_log_level);
	}
	return 0;
}

static int aml_key_config(struct aml_kt_dev *dev, struct key_config key_cfg)
{
	int ret = KT_SUCCESS;
	struct amlkt_cfg_param kt_cfg;

	switch (key_cfg.key_userid) {
	case DSC_LOC_DEC:
		key_cfg.key_userid = AML_KT_USER_TSD;
		break;
	case DSC_NETWORK:
		key_cfg.key_userid = AML_KT_USER_TSN;
		break;
	case DSC_LOC_ENC:
		key_cfg.key_userid = AML_KT_USER_TSE;
		break;
	case CRYPTO_T0:
		key_cfg.key_userid = AML_KT_USER_M2M_0;
		break;
	case CRYPTO_T1:
		key_cfg.key_userid = AML_KT_USER_M2M_1;
		break;
	case CRYPTO_T2:
		key_cfg.key_userid = AML_KT_USER_M2M_2;
		break;
	case CRYPTO_T3:
		key_cfg.key_userid = AML_KT_USER_M2M_3;
		break;
	case CRYPTO_T4:
		key_cfg.key_userid = AML_KT_USER_M2M_4;
		break;
	case CRYPTO_T5:
		key_cfg.key_userid = AML_KT_USER_M2M_5;
		break;
	case CRYPTO_ANY:
		key_cfg.key_userid = AML_KT_USER_M2M_ANY;
		break;
	default:
		KT_LOGE("%s, %d invalid user id\n", __func__, __LINE__);
		ret = KT_ERROR;
		return ret;
	}

	switch (key_cfg.key_algo) {
	case KEY_ALGO_AES:
		key_cfg.key_algo = AML_KT_ALGO_AES;
		break;
	case KEY_ALGO_TDES:
		key_cfg.key_algo = AML_KT_ALGO_TDES;
		break;
	case KEY_ALGO_DES:
		key_cfg.key_algo = AML_KT_ALGO_DES;
		break;
	case KEY_ALGO_CSA2:
		key_cfg.key_algo = AML_KT_ALGO_CSA2;
		break;
	case KEY_ALGO_CSA3:
		key_cfg.key_algo = AML_KT_ALGO_CSA3;
		break;
	case KEY_ALGO_NDL:
		key_cfg.key_algo = AML_KT_ALGO_NDL;
		break;
	case KEY_ALGO_ND:
		key_cfg.key_algo = AML_KT_ALGO_ND;
		break;
	case KEY_ALGO_S17:
		key_cfg.key_algo = AML_KT_ALGO_S17;
		break;
	case KEY_ALGO_SM4:
		key_cfg.key_algo = AML_KT_ALGO_SM4;
		break;
	default:
		KT_LOGE("%s, %d invalid algo\n", __func__, __LINE__);
		ret = KT_ERROR;
		return ret;
	}

	memset(&kt_cfg, 0, sizeof(kt_cfg));
	kt_cfg.handle = key_cfg.key_index;
	kt_cfg.key_userid = key_cfg.key_userid;
	kt_cfg.key_algo = key_cfg.key_algo;
	kt_cfg.key_flag = AML_KT_FLAG_NONE;
	kt_cfg.key_source = AML_KT_SRC_REE_HOST; //fixed
	kt_cfg.ext_value = key_cfg.ext_value;

	ret = aml_kt_config(dev, kt_cfg);

	return ret;
}

static int aml_key_set_host_key(struct aml_kt_dev *dev, struct key_descr *key_param)
{
	int ret = KT_SUCCESS;
	u32 index = 0;
	u32 key_userid = 0;
	u32 key_source = 0;
	u32 key_flag = 0;
	u8 is_iv = 0;
	struct amlkt_set_key_param kt_param;

	ret = aml_kt_handle_to_kte(dev, key_param->key_index, &index);
	if (ret != KT_SUCCESS) {
		KT_LOGE("index get error\n");
		return ret;
	}

	is_iv = key_param->key_index >> KT_IV_FLAG_OFFSET;
	if (is_iv) {
		if (!dev->kt_iv_slot[index])
			return KT_ERROR;
		key_userid = dev->kt_iv_slot[index]->key_userid;
	} else {
		if (!dev->kt_slot[index])
			return KT_ERROR;
		key_userid = dev->kt_slot[index]->key_userid;
	}

	if (key_param->key_len == 1 || key_param->key_len == 2) {
		/* key_len 1:CAS_A decrypt, 2:CAS_A encrypt */
		KT_LOGI("key_len=%d, mode=1 CAS_A\n", key_param->key_len);
		key_source = AML_KT_SRC_REE_CERT;
	}

	if (key_userid <= AML_KT_USER_M2M_ANY) {
		if (key_param->key_len == 1)
			key_flag = AML_KT_FLAG_ENC_ONLY;
		else if (key_param->key_len == 2)
			key_flag = AML_KT_FLAG_DEC_ONLY;
		else
			key_flag = AML_KT_FLAG_ENC_DEC;
	} else if (key_userid == AML_KT_USER_TSD ||
			key_userid == AML_KT_USER_TSN) {
		key_flag = AML_KT_FLAG_DEC_ONLY;
	} else {
		key_flag = AML_KT_FLAG_ENC_ONLY;
	}

	if (is_iv) {
		dev->kt_iv_slot[index]->key_flag = key_flag;
		dev->kt_iv_slot[index]->key_source = key_source;
	} else {
		dev->kt_slot[index]->key_flag = key_flag;
		dev->kt_slot[index]->key_source = key_source;
	}

	if (key_source == AML_KT_SRC_REE_CERT) {
		ret = aml_kt_set_hw_key(dev, key_param->key_index);
	} else {
		memset(&kt_param, 0, sizeof(kt_param));
		kt_param.handle = key_param->key_index;
		kt_param.key_len = key_param->key_len;
		memcpy(kt_param.key, key_param->key, key_param->key_len);
		ret = aml_kt_set_host_key(dev, &kt_param);
	}

	return ret;
}

int aml_key_open(struct inode *inode, struct file *filp)
{
	struct aml_key_dev *dev;

	dev = container_of(inode->i_cdev, struct aml_key_dev, cdev);
	filp->private_data = dev;

	return 0;
}

int aml_key_release(struct inode *inode, struct file *filp)
{
	if (!filp->private_data)
		return 0;

	filp->private_data = NULL;

	return 0;
}

static long aml_key_ioctl(struct file *filp, unsigned int cmd,
			  unsigned long arg)
{
	struct key_alloc alloc_param;
	struct key_config cfg_param;
	struct key_descr key_param;
	u32 handle = 0;
	u32 flag = 0;
	u32 key_sts = 0;
	int ret = 0;

	switch (cmd) {
	case KEY_ALLOC:
		memset(&alloc_param, 0, sizeof(alloc_param));
		if (copy_from_user(&alloc_param, (void __user *)arg,
				   sizeof(alloc_param))) {
			KT_LOGE("aml_key_alloc copy_from_user error\n");
			return -EFAULT;
		}

		if (alloc_param.is_iv)
			flag = AML_KT_ALLOC_FLAG_IV;
		ret = aml_kt_alloc(&aml_kt_dev, flag, &alloc_param.key_index);
		if (ret != 0) {
			KT_LOGE("aml_key_alloc failed retval=0x%08x\n", ret);
			return -EFAULT;
		}

		ret = copy_to_user((void __user *)arg, &alloc_param,
				   sizeof(alloc_param));
		if (unlikely(ret)) {
			KT_LOGE("aml_key_alloc copy_to_user error\n");
			return -EFAULT;
		}
		break;
	case KEY_CONFIG:
		memset(&cfg_param, 0, sizeof(cfg_param));
		if (copy_from_user(&cfg_param, (void __user *)arg,
				   sizeof(cfg_param))) {
			KT_LOGE("aml_key_config copy_from_user error\n");
			return -EFAULT;
		}

		ret = aml_key_config(&aml_kt_dev, cfg_param);
		if (ret != 0) {
			KT_LOGE("aml_key_config failed retval=0x%08x\n", ret);
			return -EFAULT;
		}
		break;
	case KEY_SET:
		memset(&key_param, 0, sizeof(key_param));
		if (copy_from_user(&key_param, (void __user *)arg,
				   sizeof(key_param))) {
			KT_LOGE("aml_key_set_host_key copy_from_user error\n");
			return -EFAULT;
		}

		if (key_param.key_len > 32) {
			KT_LOGE("key len[%d] error\n", key_param.key_len);
			return -EFAULT;
		}

		ret = aml_key_set_host_key(&aml_kt_dev, &key_param);
		if (ret != 0) {
			KT_LOGE("aml_key_set_host_key failed retval=0x%08x\n", ret);
			return -EFAULT;
		}
		break;
	case KEY_FREE:
		handle = (u32)arg;
		ret = aml_kt_free(&aml_kt_dev, handle);
		if (ret != 0) {
			KT_LOGE("aml_key_free failed retval=0x%08x\n", ret);
			return -EFAULT;
		}
		break;
	case KEY_GET_FLAG:
		memset(&key_param, 0, sizeof(key_param));
		if (copy_from_user(&key_param, (void __user *)arg,
				   sizeof(key_param))) {
			KT_LOGE("aml_kt_get_status copy_from_user error\n");
			return -EFAULT;
		}

		ret = aml_kt_get_status(&aml_kt_dev, key_param.key_index,
				       &key_sts);
		if (ret != 0) {
			KT_LOGE("aml_kt_get_status failed retval=0x%08x\n", ret);
			return -EFAULT;
		}
		memcpy(key_param.key, &key_sts, sizeof(key_sts));

		ret = copy_to_user((void __user *)arg, &key_param,
				   sizeof(key_param));
		if (unlikely(ret)) {
			KT_LOGE("aml_kt_get_status copy_to_user error\n");
			return -EFAULT;
		}
		break;
	default:
		KT_LOGE("Unknown cmd: %d\n", cmd);
		return -EFAULT;
	}

	return 0;
}

static const struct file_operations aml_key_fops = {
	.owner = THIS_MODULE,
	.open = aml_key_open,
	.release = aml_key_release,
	.unlocked_ioctl = aml_key_ioctl,
	.compat_ioctl = aml_key_ioctl,
};

int aml_key_init(struct class *aml_key_class)
{
	int ret = -1;
	struct device *device;

	if (alloc_chrdev_region(&aml_key_devt, 0, DEVICE_INSTANCES,
				AML_KEY_DEVICE_NAME) < 0) {
		KT_LOGE("%s device can't be allocated.\n", AML_KEY_DEVICE_NAME);
		return -ENXIO;
	}

	cdev_init(&aml_key_dev.cdev, &aml_key_fops);
	aml_key_dev.cdev.owner = THIS_MODULE;
	ret = cdev_add(&aml_key_dev.cdev,
		       MKDEV(MAJOR(aml_key_devt), MINOR(aml_key_devt)), 1);
	if (unlikely(ret < 0))
		goto unregister_chrdev;

	device = device_create(aml_key_class, NULL, aml_key_devt, NULL,
			       AML_KEY_DEVICE_NAME);
	if (IS_ERR(device)) {
		KT_LOGE("device_create failed\n");
		ret = PTR_ERR(device);
		goto delete_cdev;
	}

	return ret;

delete_cdev:
	cdev_del(&aml_key_dev.cdev);
unregister_chrdev:
	unregister_chrdev_region(aml_key_devt, DEVICE_INSTANCES);

	return ret;
}

void aml_key_exit(struct class *aml_key_class)
{
	device_destroy(aml_key_class, aml_key_devt);
	cdev_del(&aml_key_dev.cdev);
	unregister_chrdev_region(MKDEV(MAJOR(aml_key_devt),
				       MINOR(aml_key_devt)),
				 DEVICE_INSTANCES);
}

int __init aml_key_driver_init(void)
{
	int ret = 0;

	aml_key_class = class_create(THIS_MODULE, AML_KEY_DEVICE_NAME);
	if (IS_ERR(aml_key_class)) {
		KT_LOGE("class_create failed\n");
		ret = PTR_ERR(aml_key_class);
		return ret;
	}

	ret = aml_key_init(aml_key_class);
	if (unlikely(ret < 0)) {
		KT_LOGE("aml_key_init failed\n");
		class_destroy(aml_key_class);
		return ret;
	}

	ret = aml_key_init_dbgfs();
	if (ret) {
		KT_LOGE("aml_key_init_dbgfs failed\n");
		return ret;
	}

	return ret;
}

void __exit aml_key_driver_exit(void)
{
	aml_key_exit(aml_key_class);
	class_destroy(aml_key_class);
	debugfs_remove_recursive(aml_key_debug_dent);
}

module_init(aml_key_driver_init);
module_exit(aml_key_driver_exit);

MODULE_AUTHOR("Amlogic Inc.");
MODULE_DESCRIPTION("Old Keytable driver to be deprecated.");
MODULE_LICENSE("GPL v2");
