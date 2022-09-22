// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

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
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/amlogic/aml_key.h>
#include <linux/compat.h>
#include "am_key.h"
#include "sc2_demux/dvb_reg.h"
#include "key_reg.h"
#include "dmx_log.h"

#define IVE_BEGIN			(0)
#define IVE_MAX				(32)
#define KTE_BEGIN			(IVE_MAX)
#define KTE_MAX				(64)
#define KTE_IV_MAX			(IVE_MAX + KTE_MAX)
#define KTE_INVALID			0
#define KTE_VALID			1
#define REE_SUCCESS			0
#define KTE_KTE_MASK		0x7F

#define KTE_IV_FLAG_OFFSET (31)
#define KTE_INVALID_INDEX  (0xFF)

#define KTE_PENDING          (1)
#define KTE_MODE_READ_FLAG   (0)
#define KTE_MODE_CAS_A       (1)
#define KTE_MODE_HOST        (3)
#define KTE_CLEAN_KTE        (1)

#define KTE_STATUS_MASK      (3)
#define MKL_STS_OK           (0)

#define MKL_USER_CRYPTO_T0   (0)
#define MKL_USER_CRYPTO_T1   (1)
#define MKL_USER_CRYPTO_T2   (2)
#define MKL_USER_CRYPTO_T3   (3)
#define MKL_USER_CRYPTO_T4   (4)
#define MKL_USER_CRYPTO_T5   (5)
#define MKL_USER_CRYPTO_ANY  (7)
#define MKL_USER_LOC_DEC     (8)
#define MKL_USER_NETWORK     (9)
#define MKL_USER_LOC_ENC     (10)
/* key algorithm */
#define MKL_USAGE_AES        (0)
#define MKL_USAGE_TDES       (1)
#define MKL_USAGE_DES        (2)
#define MKL_USAGE_S17		 (3)
#define MKL_USAGE_NDL        (7)
#define MKL_USAGE_ND         (8)
#define MKL_USAGE_CSA3       (9)
#define MKL_USAGE_CSA2       (10)
#define MKL_USAGE_HMAC       (13)

/*bit 0~11: frobenious cycles*/
/*bit 12: variant cryptography
 *        0: variant 5
 *        1: variant 6
 */
 /*variant: 6, frobenious cycles:0x5a5*/
#define S17_CFG_DEFAULT     (0x15a5)

#define HANDLE_TO_KTE(h) ((h) & (KTE_MAX - 1))
struct key_table_s {
	u32 flag;		//0:invalid, 1:valid
	u32 kte;
	int key_userid;
	int key_algo;
	int is_iv;
};

static struct key_table_s key_table[KTE_IV_MAX];

static struct mutex mutex;

#define dprint_i(fmt, args...)   \
	dprintk(LOG_DBG, debug_key, fmt, ## args)
#define dprint(fmt, args...)    \
	dprintk(LOG_ERROR, debug_key, "key:" fmt, ## args)
#define pr_dbg(fmt, args...)    \
	dprintk(LOG_DBG, debug_key, "key:" fmt, ## args)

MODULE_PARM_DESC(debug_key, "\n\t\t Enable key debug information");
static int debug_key;
module_param(debug_key, int, 0644);

static struct class *dmx_key_class;
static int major_key;

static int find_kt_index(u32 handle)
{
	u32 is_iv = 0;
	u32 kte;
	int i;

	is_iv = handle >> KTE_IV_FLAG_OFFSET;
	kte = HANDLE_TO_KTE(handle);

	for (i = 0; i < KTE_IV_MAX; i++) {
		if (key_table[i].kte == kte &&
		    key_table[i].is_iv == is_iv &&
		    key_table[i].flag == KTE_VALID) {
			return i;
		}
	}

	return -1;
}

static int kt_clean(int index)
{
	int i = 0, res = REE_SUCCESS;
	u32 kte;

	if (index < 0 || index >= KTE_IV_MAX) {
		pr_dbg("%s, %d index invalid:%d\n", __func__, __LINE__, index);
		return -1;
	}
	if (key_table[index].flag != KTE_VALID) {
		pr_dbg("%s, %d kte flag invalid\n",	__func__, __LINE__);
		return -1;
	}

	kte = key_table[index].kte;
	if (READ_CBUS_REG(KT_REE_RDY) == 0) {
//              dprint("%s, %d not ready\n", __func__, __LINE__);
		return -1;
	}
	WRITE_CBUS_REG(KT_REE_CFG, (KTE_PENDING << KTE_PENDING_OFFSET)
		       | (KTE_CLEAN_KTE << KTE_CLEAN_OFFSET)
		       | (KTE_MODE_HOST << KTE_MODE_OFFSET)
		       | (kte << KTE_KTE_OFFSET));
	do {
		res = READ_CBUS_REG(KT_REE_CFG);
		if (i++ > 800) {
			dprint("KT_REE_CFG still pending. timed out\n");
			WRITE_CBUS_REG(KT_REE_CFG, (0 << KTE_PENDING_OFFSET));
			WRITE_CBUS_REG(KT_REE_RDY, 1);
			return -1;
		}
		usleep_range(10000, 15000);
	} while (res & (KTE_PENDING << KTE_PENDING_OFFSET));
	pr_dbg("KT_REE_CFG=0x%08x\n", res);

	WRITE_CBUS_REG(KT_REE_RDY, 1);
	if (((res >> KTE_STATUS_OFFSET) & KTE_STATUS_MASK) == MKL_STS_OK) {
		key_table[index].flag = KTE_INVALID;
		res = REE_SUCCESS;
	} else {
//              dprint("%s, clean key index fail\n", __func__);
		res = -1;
	}
	return res;
}

static int kt_init(void)
{
	int i;

	for (i = 0; i < KTE_IV_MAX; i++) {
		key_table[i].flag = KTE_INVALID;
		key_table[i].kte = (i >= IVE_MAX ? (i - IVE_MAX) : i);
		key_table[i].is_iv = (i < IVE_MAX ? 1 : 0);
	}

	return REE_SUCCESS;
}

static int kt_alloc(u32 *handle, struct key_alloc *config)
{
	int res = REE_SUCCESS;
	int i;
	int begin;
	int end;
	int is_iv = 0;

	if (!handle)
		return -1;

	pr_dbg("%s iv:%d\n", __func__, config->is_iv);
	if (config->is_iv) {
		begin = IVE_BEGIN;
		end = IVE_MAX;
		is_iv = 1;
	} else {
		begin = KTE_BEGIN;
		end = KTE_IV_MAX;
	}

	for (i = begin; i < end; i++) {
		if (key_table[i].flag == KTE_INVALID) {
			key_table[i].flag = KTE_VALID;
			*handle = key_table[i].kte |
				(is_iv << KTE_IV_FLAG_OFFSET);
			break;
		}
	}

	if (i == end) {
		dprint("%s, %d key_iv slot full\n", __func__, __LINE__);
		res = -1;
	} else {
		res = REE_SUCCESS;
	}
	return res;
}

static int kt_config(u32 handle, int key_userid, int key_algo, unsigned int ext_value)
{
	int res = REE_SUCCESS;
	int index;
	u32 kte;

	kte = HANDLE_TO_KTE(handle);
	index = find_kt_index(handle);

	if (index == -1) {
		dprint("%s, handle:%#x index invalid\n", __func__, handle);
		return -1;
	}

	pr_dbg("%s user_id:%d, key_algo:%d\n", __func__, key_userid, key_algo);

	switch (key_userid) {
	case DSC_LOC_DEC:
		key_table[index].key_userid = MKL_USER_LOC_DEC;
		break;
	case DSC_NETWORK:
		key_table[index].key_userid = MKL_USER_NETWORK;
		break;
	case DSC_LOC_ENC:
		key_table[index].key_userid = MKL_USER_LOC_ENC;
		break;
	case CRYPTO_T0:
		key_table[index].key_userid = MKL_USER_CRYPTO_T0;
		break;
	case CRYPTO_T1:
		key_table[index].key_userid = MKL_USER_CRYPTO_T1;
		break;
	case CRYPTO_T2:
		key_table[index].key_userid = MKL_USER_CRYPTO_T2;
		break;
	case CRYPTO_T3:
		key_table[index].key_userid = MKL_USER_CRYPTO_T3;
		break;
	case CRYPTO_T4:
		key_table[index].key_userid = MKL_USER_CRYPTO_T4;
		break;
	case CRYPTO_T5:
		key_table[index].key_userid = MKL_USER_CRYPTO_T5;
		break;
	case CRYPTO_ANY:
		key_table[index].key_userid = MKL_USER_CRYPTO_ANY;
		break;
	default:
		dprint("%s, %d invalid user id\n",
		       __func__, __LINE__);
		return -1;
	}
	if (key_table[index].is_iv == 0) {
		switch (key_algo) {
		case KEY_ALGO_AES:
			key_table[index].key_algo = MKL_USAGE_AES;
			break;
		case KEY_ALGO_TDES:
			key_table[index].key_algo = MKL_USAGE_TDES;
			break;
		case KEY_ALGO_DES:
			key_table[index].key_algo = MKL_USAGE_DES;
			break;
		case KEY_ALGO_CSA2:
			key_table[index].key_algo = MKL_USAGE_CSA2;
			break;
		case KEY_ALGO_CSA3:
			key_table[index].key_algo = MKL_USAGE_CSA3;
			break;
		case KEY_ALGO_NDL:
			key_table[index].key_algo = MKL_USAGE_NDL;
			break;
		case KEY_ALGO_ND:
			key_table[index].key_algo = MKL_USAGE_ND;
			break;
		case KEY_ALGO_S17:
			key_table[index].key_algo = MKL_USAGE_S17;
			if (key_userid < CRYPTO_T0) {
				if (ext_value == 0) {
					WRITE_CBUS_REG(KT_REE_S17_CONFIG,
						       S17_CFG_DEFAULT);
					dprint_i("def frobenius :0x%0x\n",
						 S17_CFG_DEFAULT);
				} else {
					WRITE_CBUS_REG(KT_REE_S17_CONFIG,
						       ext_value);
					dprint_i("frobenius :0x%0x\n",
						 ext_value);
				}
			}
			break;
		default:
			dprint("%s, %d invalid algo\n",
			       __func__, __LINE__);
			return -1;
		}
	}			else {
		key_table[index].key_algo = 0xf;
	}

	return res;
}

static int kt_set(u32 handle, unsigned char key[32], unsigned int key_len)
{
	int res = REE_SUCCESS;
	u32 KT_KEY0, KT_KEY1, KT_KEY2, KT_KEY3;
	u32 key0 = 0, key1 = 0, key2 = 0, key3 = 0;
	u32 key4 = 0, key5 = 0, key6 = 0, key7 = 0;
	int user_id = 0;
	int algo = 0;
	int en_decrypt = 0;
	int index;
	u32 kte;
	u32 mode = KTE_MODE_HOST;
	int i;
	int fun_id;

	kte = HANDLE_TO_KTE(handle);
	index = find_kt_index(handle);

	if (index == -1) {
		dprint("%s, handle:%#x index invalid\n", __func__, handle);
		return -1;
	}
//{
//	int i = 0;
//	dprint_i("kte:%d, len:%d key:\n", kte, key_len);
//	for (i = 0; i < key_len; i++)
//		dprint_i("0x%0x ", key[i]);
//	dprint_i("\n");
//}
	if (key_table[index].flag != KTE_VALID) {
		dprint("%s, %d kte flag invalid\n", __func__, __LINE__);
		return -1;
	}

	/*KEY_FROM_REE_HOST: */
	KT_KEY0 = KT_REE_KEY0;
	KT_KEY1 = KT_REE_KEY1;
	KT_KEY2 = KT_REE_KEY2;
	KT_KEY3 = KT_REE_KEY3;

	if (key_len == 1 || key_len == 2) {
		/* key_len 1:CAS_A decrypt, 2:CAS_A encrypt */
		dprint_i("driver aml_key key_len=%d, mode=1 CAS_A\n", key_len);
		mode = KTE_MODE_CAS_A;
	}

	if (key_len >= 4)
		memcpy((void *)&key0, &key[0], 4);
	if (key_len >= 8)
		memcpy((void *)&key1, &key[4], 4);
	if (key_len >= 12)
		memcpy((void *)&key2, &key[8], 4);
	if (key_len >= 16)
		memcpy((void *)&key3, &key[12], 4);
	if (key_len >= 20)
		memcpy((void *)&key4, &key[16], 4);
	if (key_len >= 24)
		memcpy((void *)&key5, &key[20], 4);
	if (key_len >= 28)
		memcpy((void *)&key6, &key[24], 4);
	if (key_len >= 32)
		memcpy((void *)&key7, &key[28], 4);


	user_id = key_table[index].key_userid;
	if (user_id <= MKL_USER_CRYPTO_ANY) {
		if (key_len == 1) {
			en_decrypt = 1;
		} else if (key_len == 2) {
			en_decrypt = 2;
		} else {
			/* if from AP, enable for both encrypt and decrypt */
			en_decrypt = 3;
		}
	} else if (user_id == MKL_USER_LOC_DEC || user_id == MKL_USER_NETWORK) {
		en_decrypt = 2;
	} else {
		en_decrypt = 1;
	}

	algo = key_table[index].key_algo;
	i = 0;
	while (READ_CBUS_REG(KT_REE_RDY) == 0) {
		if (i++ > 10) {
			dprint("%s, %d not ready\n", __func__, __LINE__);
			return -1;
		}
		usleep_range(10000, 15000);
	}
	if (key_len == 32 && (algo == MKL_USAGE_S17 || algo == MKL_USAGE_AES)) {
		fun_id = 6;
		WRITE_CBUS_REG(KT_KEY0, key0);
		WRITE_CBUS_REG(KT_KEY1, key1);
		WRITE_CBUS_REG(KT_KEY2, key2);
		WRITE_CBUS_REG(KT_KEY3, key3);
		WRITE_CBUS_REG(KT_REE_CFG, (KTE_PENDING << KTE_PENDING_OFFSET)
			| (mode << KTE_MODE_OFFSET)
			| (en_decrypt << KTE_FLAG_OFFSET)
			| (algo << KTE_KEYALGO_OFFSET)
			| (user_id << KTE_USERID_OFFSET)
			| (kte << KTE_KTE_OFFSET)
			| (0 << KTE_TEE_PRIV_OFFSET)
			| (0 << KTE_LEVEL_OFFSET)
			| fun_id);
		i = 0;
		do {
			res = READ_CBUS_REG(KT_REE_CFG);
			if (i++ > 800) {
				dprint("KT_REE_CFG still pending. timed out\n");
				WRITE_CBUS_REG(KT_REE_CFG, (0 << KTE_PENDING_OFFSET));
				WRITE_CBUS_REG(KT_REE_RDY, 1);
				return -1;
			}
			usleep_range(10000, 15000);
		} while (res & (KTE_PENDING << KTE_PENDING_OFFSET));

		fun_id = 7;
		WRITE_CBUS_REG(KT_KEY0, key4);
		WRITE_CBUS_REG(KT_KEY1, key5);
		WRITE_CBUS_REG(KT_KEY2, key6);
		WRITE_CBUS_REG(KT_KEY3, key7);
		WRITE_CBUS_REG(KT_REE_CFG, (KTE_PENDING << KTE_PENDING_OFFSET)
			| (mode << KTE_MODE_OFFSET)
			| (en_decrypt << KTE_FLAG_OFFSET)
			| (algo << KTE_KEYALGO_OFFSET)
			| (user_id << KTE_USERID_OFFSET)
			| (kte << KTE_KTE_OFFSET)
			| (0 << KTE_TEE_PRIV_OFFSET)
			| (0 << KTE_LEVEL_OFFSET)
			| fun_id);
		i = 0;
		do {
			res = READ_CBUS_REG(KT_REE_CFG);
			if (i++ > 800) {
				dprint("KT_REE_CFG still pending. timed out\n");
				WRITE_CBUS_REG(KT_REE_CFG, (0 << KTE_PENDING_OFFSET));
				WRITE_CBUS_REG(KT_REE_RDY, 1);
				return -1;
			}
			usleep_range(10000, 15000);
		} while (res & (KTE_PENDING << KTE_PENDING_OFFSET));
	} else {
		if (get_cpu_type() == MESON_CPU_MAJOR_ID_T5W) {
			if (algo != 0xf) {
				if (algo == MKL_USAGE_AES)
					algo = 2;
				else if (algo == MKL_USAGE_TDES)
					algo = 1;
				else /*csa2 and des is 0*/
					algo = 0;
			}
		}
		WRITE_CBUS_REG(KT_KEY0, key0);
		WRITE_CBUS_REG(KT_KEY1, key1);
		WRITE_CBUS_REG(KT_KEY2, key2);
		WRITE_CBUS_REG(KT_KEY3, key3);
		WRITE_CBUS_REG(KT_REE_CFG, (KTE_PENDING << KTE_PENDING_OFFSET)
			| (mode << KTE_MODE_OFFSET)
			| (en_decrypt << KTE_FLAG_OFFSET)
			| (algo << KTE_KEYALGO_OFFSET)
			| (user_id << KTE_USERID_OFFSET)
			| (kte << KTE_KTE_OFFSET)
			| (0 << KTE_TEE_PRIV_OFFSET)
			| (0 << KTE_LEVEL_OFFSET));
		i = 0;
		do {
			res = READ_CBUS_REG(KT_REE_CFG);
			if (i++ > 800) {
				dprint("KT_REE_CFG still pending. timed out\n");
				WRITE_CBUS_REG(KT_REE_CFG, (0 << KTE_PENDING_OFFSET));
				WRITE_CBUS_REG(KT_REE_RDY, 1);
				return -1;
			}
			usleep_range(10000, 15000);
		} while (res & (KTE_PENDING << KTE_PENDING_OFFSET));
	};
	pr_dbg("KT_CFG[0x%08x]=0x%08x\n", KT_REE_CFG, res);
	WRITE_CBUS_REG(KT_REE_RDY, 1);
	if (((res >> KTE_STATUS_OFFSET) & KTE_STATUS_MASK) == MKL_STS_OK) {
		res = REE_SUCCESS;
	} else {
		dprint("%s, fail\n", __func__);
		res = -1;
	}
	return res;
}

static int kt_get_flag(u32 handle, unsigned char key_flag[32], unsigned int len)
{
	int res = REE_SUCCESS;
	int user_id = 0;
	int algo = 0;
	int en_decrypt = 0;
	u32 flag = 0;
	int i, index;
	u32 kte;

	kte = HANDLE_TO_KTE(handle);
	index = find_kt_index(handle);

	if (index == -1) {
		dprint("%s, handle:%#x index invalid\n", __func__, handle);
		return -1;
	}

	if (kte >= KTE_IV_MAX) {
		dprint("%s,kte:%d invalid\n", __func__, kte);
		return -1;
	}
	if (key_table[index].flag != KTE_VALID) {
		dprint("%s, %d kte flag invalid\n", __func__, __LINE__);
		return -1;
	}

	user_id = key_table[index].key_userid;
	algo = key_table[index].key_algo;

	i = 0;
	while (READ_CBUS_REG(KT_REE_RDY) == 0) {
		if (i++ > 10) {
			dprint("not ready, not getting flag\n");
			return -1;
		}
		usleep_range(10000, 15000);
	}
	WRITE_CBUS_REG(KT_REE_CFG, (KTE_PENDING << KTE_PENDING_OFFSET)
			| (KTE_MODE_READ_FLAG << KTE_MODE_OFFSET)
			| (en_decrypt << KTE_FLAG_OFFSET)
			| (algo << KTE_KEYALGO_OFFSET)
			| (user_id << KTE_USERID_OFFSET)
			| (kte << KTE_KTE_OFFSET)
			| (0 << KTE_TEE_PRIV_OFFSET)
			| (0 << KTE_LEVEL_OFFSET));
	i = 0;
	do {
		if (i++ > 10) {
			dprint("timed out\n");
			WRITE_CBUS_REG(KT_REE_CFG, (0 << KTE_PENDING_OFFSET));
			WRITE_CBUS_REG(KT_REE_RDY, 1);
			return -1;
		}
		res = READ_CBUS_REG(KT_REE_CFG);
	} while (res & (KTE_PENDING << KTE_PENDING_OFFSET));
	flag = READ_CBUS_REG(KT_REE_STS);
	memcpy(&key_flag[0], &flag, sizeof(flag));
	WRITE_CBUS_REG(KT_REE_RDY, 1);
	return 0;
}

int kt_free(u32 handle)
{
	int index;

	index = find_kt_index(handle);
//      pr_dbg("%s kte:%d enter\n", __func__, kte);
	if (index == -1) {
		dprint("%s, handle:%#x index invalid\n", __func__, handle);
		return -1;
	}

	return kt_clean(index);
}

static int usercopy(struct file *file,
		    unsigned int cmd, unsigned long arg,
		    int (*func)(struct file *file,
				unsigned int cmd, void *arg))
{
	char sbuf[128];
	void *mbuf = NULL;
	void *parg = NULL;
	int err = -EINVAL;

	/*  Copy arguments into temp kernel buffer  */
	switch (_IOC_DIR(cmd)) {
	case _IOC_NONE:
		/*
		 * For this command, the pointer is actually an integer
		 * argument.
		 */
		parg = (void *)arg;
		break;
	case _IOC_READ:	/* some v4l ioctls are marked wrong ... */
	case _IOC_WRITE:
	case (_IOC_WRITE | _IOC_READ):
		if (_IOC_SIZE(cmd) <= sizeof(sbuf)) {
			parg = sbuf;
		} else {
			/* too big to allocate from stack */
			mbuf = kmalloc(_IOC_SIZE(cmd), GFP_KERNEL);
			if (!mbuf)
				return -ENOMEM;
			parg = mbuf;
		}

		err = -EFAULT;
		if (copy_from_user(parg, (void __user *)arg, _IOC_SIZE(cmd)))
			goto out;
		break;
	}

	/* call driver */
	err = func(file, cmd, parg);
	if (err == -ENOIOCTLCMD)
		err = -ENOTTY;

	if (err < 0)
		goto out;

	/*  Copy results into user buffer  */
	switch (_IOC_DIR(cmd)) {
	case _IOC_READ:
	case (_IOC_WRITE | _IOC_READ):
		if (copy_to_user((void __user *)arg, parg, _IOC_SIZE(cmd)))
			err = -EFAULT;
		break;
	}

out:
	kfree(mbuf);
	return err;
}

int dmx_key_open(struct inode *inode, struct file *file)
{
	mutex_lock(&mutex);

	file->private_data = vmalloc(KTE_IV_MAX);
	if (file->private_data)
		memset(file->private_data, KTE_INVALID_INDEX, KTE_IV_MAX);
	pr_dbg("%s line:%d\n", __func__, __LINE__);
	mutex_unlock(&mutex);

	return 0;
}

int dmx_key_close(struct inode *inode, struct file *file)
{
	int i = 0;
	char index = 0;

	mutex_lock(&mutex);
	if (file->private_data) {
		for (i = 0; i < KTE_IV_MAX; i++) {
			index = *(char *)(file->private_data + i);
			if (index != KTE_INVALID_INDEX) {
				kt_clean(index);
				*(char *)(file->private_data + i) =
					KTE_INVALID_INDEX;
			}
		}
		vfree(file->private_data);
		file->private_data = NULL;
	}
	mutex_unlock(&mutex);
	pr_dbg("%s line:%d\n", __func__, __LINE__);
	return 0;
}

int dmx_key_do_ioctl(struct file *file, unsigned int cmd, void *parg)
{
	int ret = -EINVAL;
	char *p = file->private_data;

	mutex_lock(&mutex);
	switch (cmd) {
	case KEY_ALLOC:{
			u32 kte = 0;
			int idx;
			struct key_alloc *d = parg;

			if (kt_alloc(&kte, d) == 0) {
				d->key_index = kte;
				ret = 0;
				idx = find_kt_index(kte);
				if (p && idx != -1)
					*(char *)(p + idx) =
						idx;
			}
			break;
		}
	case KEY_FREE:{
			u32 kte = (unsigned long)parg;
			int idx;

			if (kt_free(kte) == 0) {
				ret = 0;
				idx = find_kt_index(kte);
				if (p && idx != -1)
					*(char *)(p + idx) =
						KTE_INVALID_INDEX;
			}
			break;
		}
	case KEY_SET:{
			struct key_descr *d = parg;

			if (kt_set(d->key_index, d->key, d->key_len) == 0)
				ret = 0;
			break;
		}
	case KEY_CONFIG:{
			struct key_config *config = parg;

			if (kt_config(config->key_index,
				config->key_userid, config->key_algo, config->ext_value) == 0)
				ret = 0;
			break;
		}
	case KEY_GET_FLAG:{
			struct key_descr *d = parg;

			if (kt_get_flag(d->key_index, d->key, d->key_len) == 0)
				ret = 0;
			break;
		}
	default:
		break;
	}

	mutex_unlock(&mutex);

	return 0;
}

long dmx_key_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return usercopy(file, cmd, arg, dmx_key_do_ioctl);
}

#ifdef CONFIG_COMPAT
static long _dmx_key_compat_ioctl(struct file *filp,
				  unsigned int cmd, unsigned long args)
{
	unsigned long ret;

	args = (unsigned long)compat_ptr(args);
	ret = dmx_key_ioctl(filp, cmd, args);
	return ret;
}
#endif

static const struct file_operations device_fops = {
	.owner = THIS_MODULE,
	.open = dmx_key_open,
	.release = dmx_key_close,
	.read = NULL,
	.write = NULL,
	.poll = NULL,
	.unlocked_ioctl = dmx_key_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = _dmx_key_compat_ioctl,
#endif
};

int dmx_key_init(void)
{
	struct device *clsdev;

	major_key = register_chrdev(0, "key", &device_fops);
	if (major_key < 0) {
		dprint("error:can not register key device\n");
		return -1;
	}

	if (!dmx_key_class) {
		dmx_key_class = class_create(THIS_MODULE, "key");
		if (IS_ERR(dmx_key_class)) {
			dprint("class create dmx_key_class fail\n");
			return -1;
		}
	}
	clsdev = device_create(dmx_key_class, NULL,
			       MKDEV(major_key, 0), NULL, "key");
	if (IS_ERR(clsdev)) {
		dprint("device_create key fail\n");
		return PTR_ERR(clsdev);
	}
	mutex_init(&mutex);
	kt_init();
	dprint("%s success\n", __func__);
	return 0;
}

void dmx_key_exit(void)
{
	pr_dbg("%s enter\n", __func__);

	device_destroy(dmx_key_class, MKDEV(major_key, 0));
	unregister_chrdev(major_key, "key");

	class_destroy(dmx_key_class);
	dmx_key_class = NULL;
}
