// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* A part of the code is taken from crypto dev
 * https://github.com/cryptodev-linux/cryptodev-linux.git
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <linux/crypto.h>
#include <crypto/aes.h>
#include <crypto/sm4.h>
#include <crypto/des.h>
#include <linux/miscdevice.h>
#include <linux/of_platform.h>
#include <linux/arm-smccc.h>
#include <linux/aml_crypto.h>
#include <linux/compat.h>
#include "aml-crypto-dma.h"

#define USE_BUSY_POLLING (0)
#define MAX_IV_SIZE (32)
#define CRYPTO_OPERATION_CFG  (0x8200002A)
#define SET_S17_M2M_CFG (0x0)

#define TEST_PHYSICAL_PROCESS (0)
#if TEST_PHYSICAL_PROCESS
dma_addr_t g_src_dma[MAX_CRYPTO_BUFFERS];
u8 *g_src[MAX_CRYPTO_BUFFERS];
u8 *g_tmp_src_addr[MAX_CRYPTO_BUFFERS];
dma_addr_t g_dst_dma[MAX_CRYPTO_BUFFERS];
u8 *g_dst[MAX_CRYPTO_BUFFERS];
u8 *g_tmp_dst_addr[MAX_CRYPTO_BUFFERS];
#endif

struct aml_crypto_dev {
	struct device *dev;
	int           irq;
	u32      err;
	u32      dma_busy;
	struct mutex  lock; /* lock to protect crypto device */

	u32      thread;
	u32      status;

	u32 algo_cap;

	wait_queue_head_t waiter;
	struct task_struct *processing;
};

struct aml_crypto_dev *crypto_dd;

struct meson_crypto_dev_data {
	u32 status;
	u32 algo_cap;
};

struct meson_crypto_dev_data meson_sc2_data = {
	.status = TXLX_DMA_STS0,
	.algo_cap = (CAP_AES | CAP_TDES | CAP_DES),
};

struct meson_crypto_dev_data meson_s4d_data = {
	.status = TXLX_DMA_STS0,
	.algo_cap = (CAP_AES | CAP_TDES | CAP_DES | CAP_S17),
};

struct meson_crypto_dev_data meson_s5_data = {
	.status = TXLX_DMA_STS0,
	.algo_cap = (CAP_AES | CAP_TDES | CAP_DES | CAP_SM4),
};

struct cipher_data {
	int init; /* 0 if uninitialized */
	int cipher;
	int blocksize;
	int ivsize;
	int kte;
	int keylen;
	int op_mode;
	int crypt_mode;
};

struct crypto_session {
	struct list_head entry;
	struct mutex sem; /* lock to protect crypto session */
	u32 sid;
	struct cipher_data cdata;
};

struct fcrypt {
	struct list_head list;
	struct mutex sem; /* lock to protect fcrypt list */
};

struct crypt_priv {
	struct fcrypt fcrypt;
};

#ifdef CONFIG_COMPAT
struct compat_crypt_mem {
	__u32    length;
	compat_uptr_t addr;
};

struct compat_crypt_op {
	__u32  ses;                  /* session identifier */
	__u8   op;                   /* OP_ENCRYPT or OP_DECRYPT */
	__u8   src_phys;             /* set if src is in physical addr */
	__u8   dst_phys;             /* set if dst is in physical addr */
	__u8   ivlen;                /* length of IV */
	compat_uptr_t __user iv;
	compat_uptr_t __user param;   /* extra parameters for algorithm */
	__u16  param_len;
	__u8   num_src_bufs;
	__u8   num_dst_bufs;
	__u32  reserved;
	struct compat_crypt_mem src[MAX_CRYPTO_BUFFERS];   /* source data */
	struct compat_crypt_mem dst[MAX_CRYPTO_BUFFERS];   /* output data */
};

#define DO_CRYPTO_COMPAT          _IOWR('a', 2, struct compat_crypt_op)
#endif

struct kernel_crypt_op {
	struct crypt_op cop;

	__u8 *param;
	__u8 iv[MAX_IV_SIZE];
	u16 param_len;
	u8 ivlen;
};

static noinline int call_smc(u64 func_id, u64 arg0, u64 arg1, u64 arg2)
{
	struct arm_smccc_res res;

	arm_smccc_smc((unsigned long)func_id,
		      (unsigned long)arg0,
		      (unsigned long)arg1,
		      (unsigned long)arg2,
		      0, 0, 0, 0, &res);
	return res.a0;
}

#if !USE_BUSY_POLLING
static irqreturn_t aml_crypto_dev_irq(int irq, void *dev_id)
{
	struct aml_crypto_dev *dd = dev_id;
	struct device *dev = dd->dev;
	u8 status = aml_read_crypto_reg(dd->status);

	if (status) {
		if (status == 0x1)
			dev_err(dev, "irq overwrite\n");
		if (dd->dma_busy) {
			if (status & DMA_STATUS_KEY_ERROR) {
				dev_err(dev, "crypto device failed to fetch key\n");
				dd->err = DMA_STATUS_KEY_ERROR;
			} else {
				dd->err = 0;
			}
			aml_write_crypto_reg(dd->status, 0xf);
			wake_up_process(dd->processing);
			return IRQ_HANDLED;
		} else {
			return IRQ_NONE;
		}
	}

	return IRQ_NONE;
}
#endif

#ifdef CONFIG_OF
static const struct of_device_id aml_crypto_dev_dt_match[] = {
	{	.compatible = "amlogic,crypto_sc2",
		.data = &meson_sc2_data,
	},
	{	.compatible = "amlogic,crypto_s4d",
		.data = &meson_s4d_data,
	},
	{	.compatible = "amlogic,crypto_s5",
		.data = &meson_s5_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, aml_crypto_dev_dt_match);
#else
#define aml_crypto_dev_dt_match NULL
#endif

static int crypto_create_session(struct fcrypt *fcr, struct session_op *sop)
{
	struct crypto_session *ses_new = NULL;
	struct crypto_session *ses_ptr = NULL;
	u32 retry = 0;
	int done = 0;
	int rc = 0;

	if (sop->cipher >= CRYPTO_OP_MAX || sop->cipher == CRYPTO_OP_INVALID)
		return -EINVAL;
	if (sop->keylen > AES_MAX_KEY_SIZE)
		return -EINVAL;

	ses_new = kzalloc(sizeof(*ses_new), GFP_KERNEL);
	if (!ses_new)
		return -ENOMEM;

	get_random_bytes(&ses_new->sid, sizeof(ses_new->sid));
	mutex_init(&ses_new->sem);

	ses_new->cdata.cipher = sop->cipher;
	switch (ses_new->cdata.cipher) {
	case CRYPTO_OP_DES_ECB:
		ses_new->cdata.op_mode = OP_MODE_ECB;
		break;
	case CRYPTO_OP_DES_CBC:
		ses_new->cdata.op_mode = OP_MODE_CBC;
		break;
	case CRYPTO_OP_TDES_ECB:
		ses_new->cdata.op_mode = OP_MODE_ECB;
		break;
	case CRYPTO_OP_TDES_CBC:
		ses_new->cdata.op_mode = OP_MODE_CBC;
		break;
	case CRYPTO_OP_AES_ECB:
		ses_new->cdata.op_mode = OP_MODE_ECB;
		break;
	case CRYPTO_OP_AES_CBC:
		ses_new->cdata.op_mode = OP_MODE_CBC;
		break;
	case CRYPTO_OP_AES_CTR:
		ses_new->cdata.op_mode = OP_MODE_CTR;
		break;
	case CRYPTO_OP_S17_ECB:
		ses_new->cdata.op_mode = OP_MODE_ECB;
		break;
	case CRYPTO_OP_S17_CBC:
		ses_new->cdata.op_mode = OP_MODE_CBC;
		break;
	case CRYPTO_OP_S17_CTR:
		ses_new->cdata.op_mode = OP_MODE_CTR;
		break;
	case CRYPTO_OP_SM4_ECB:
		ses_new->cdata.op_mode = OP_MODE_ECB;
		break;
	case CRYPTO_OP_SM4_CBC:
		ses_new->cdata.op_mode = OP_MODE_CBC;
		break;
	case CRYPTO_OP_SM4_CTR:
		ses_new->cdata.op_mode = OP_MODE_CTR;
		break;
	default:
		rc = -EINVAL;
		goto error;
	}

	switch (ses_new->cdata.cipher) {
	case CRYPTO_OP_DES_ECB:
		fallthrough;
	case CRYPTO_OP_DES_CBC:
		ses_new->cdata.blocksize = DES_BLOCK_SIZE;
		ses_new->cdata.ivsize = DES_BLOCK_SIZE;
		if (sop->keylen != DES_BLOCK_SIZE) {
			dbgp(2, "invalid keysize: %d\n", sop->keylen);
			rc = -EINVAL;
			goto error;
		} else {
			ses_new->cdata.keylen = sop->keylen;
		}
		ses_new->cdata.crypt_mode = MODE_DES;
		if (!(crypto_dd->algo_cap & CAP_DES)) {
			dbgp(2, "unsupported algo: %d\n",
			     ses_new->cdata.cipher);
			rc = -EINVAL;
			goto error;
		}
		break;
	case CRYPTO_OP_TDES_ECB:
		fallthrough;
	case CRYPTO_OP_TDES_CBC:
		ses_new->cdata.blocksize = DES_BLOCK_SIZE;
		ses_new->cdata.ivsize = DES_BLOCK_SIZE;
		if (sop->keylen != DES_BLOCK_SIZE * 2 &&
		    sop->keylen != DES_BLOCK_SIZE * 3) {
			dbgp(2, "invalid keysize: %d\n", sop->keylen);
			rc = -EINVAL;
			goto error;
		} else {
			ses_new->cdata.keylen = sop->keylen;
		}
		ses_new->cdata.crypt_mode =
			(ses_new->cdata.keylen == DES_BLOCK_SIZE * 2) ?
			 MODE_TDES_2K : MODE_TDES_3K;
		if (!(crypto_dd->algo_cap & CAP_TDES)) {
			dbgp(2, "unsupported algo: %d\n",
			     ses_new->cdata.cipher);
			rc = -EINVAL;
			goto error;
		}
		break;
	case CRYPTO_OP_AES_ECB:
		fallthrough;
	case CRYPTO_OP_AES_CBC:
		fallthrough;
	case CRYPTO_OP_AES_CTR:
		ses_new->cdata.blocksize = AES_BLOCK_SIZE;
		ses_new->cdata.ivsize = AES_BLOCK_SIZE;
		/* HW only supports AES-128 and AES-256 */
		if (sop->keylen != AES_BLOCK_SIZE &&
		    sop->keylen != AES_BLOCK_SIZE * 2) {
			dbgp(2, "invalid keysize: %d\n", sop->keylen);
			rc = -EINVAL;
			goto error;
		} else {
			ses_new->cdata.keylen = sop->keylen;
		}
		ses_new->cdata.crypt_mode = (ses_new->cdata.keylen ==
					     AES_BLOCK_SIZE) ?  MODE_AES128 :
					     MODE_AES256;
		if (!(crypto_dd->algo_cap & CAP_AES)) {
			dbgp(2, "unsupported algo: %d\n",
			     ses_new->cdata.cipher);
			rc = -EINVAL;
			goto error;
		}
		break;
	case CRYPTO_OP_SM4_ECB:
		fallthrough;
	case CRYPTO_OP_SM4_CBC:
		fallthrough;
	case CRYPTO_OP_SM4_CTR:
		ses_new->cdata.blocksize = SM4_BLOCK_SIZE;
		ses_new->cdata.ivsize = SM4_BLOCK_SIZE;
		if (sop->keylen != SM4_BLOCK_SIZE) {
			dbgp(2, "invalid keysize: %d\n", sop->keylen);
			rc = -EINVAL;
			goto error;
		} else {
			ses_new->cdata.keylen = sop->keylen;
		}
		ses_new->cdata.crypt_mode = MODE_SM4;
		if (!(crypto_dd->algo_cap & CAP_SM4)) {
			dbgp(2, "unsupported algo: %d\n",
			     ses_new->cdata.cipher);
			rc = -EINVAL;
			goto error;
		}
		break;
	case CRYPTO_OP_S17_ECB:
		fallthrough;
	case CRYPTO_OP_S17_CBC:
		fallthrough;
	case CRYPTO_OP_S17_CTR:
		if (sop->keylen != 32) {
			dbgp(2, "invalid keysize: %d\n", sop->keylen);
			rc = -EINVAL;
			goto error;
		} else {
			ses_new->cdata.keylen = sop->keylen;
		}
		ses_new->cdata.blocksize = 16;
		ses_new->cdata.ivsize = 32;
		ses_new->cdata.crypt_mode = MODE_S17;
		if (!(crypto_dd->algo_cap & CAP_S17)) {
			dbgp(2, "unsupported algo: %d\n",
			     ses_new->cdata.cipher);
			rc = -EINVAL;
			goto error;
		}
		break;
	default:
		rc = -EINVAL;
		goto error;
	}
	ses_new->cdata.kte = sop->kte;
	ses_new->cdata.init = 1;

	mutex_lock(&fcr->sem);

	do {
		list_for_each_entry(ses_ptr, &fcr->list, entry) {
			/* Check for duplicate SID */
			if (unlikely(ses_new->sid == ses_ptr->sid)) {
				get_random_bytes(&ses_new->sid,
						 sizeof(ses_new->sid));
				retry++;
				break;
			}
		}
		done = 1;
	} while (retry < 20 && !done);

	if (!done) {
		dbgp(2, "failed to find a good session ID\n");
		rc = -EINVAL;
	}

	list_add(&ses_new->entry, &fcr->list);
	mutex_unlock(&fcr->sem);

	/* Fill in some values for the user. */
	sop->ses = ses_new->sid;
	return rc;

error:
	kfree(ses_new);
	return rc;
}

static int crypto_finish_session(struct fcrypt *fcr, u32 sid)
{
	struct crypto_session *tmp, *ses_ptr;
	struct list_head *head;
	int ret = 0;

	mutex_lock(&fcr->sem);
	head = &fcr->list;
	list_for_each_entry_safe(ses_ptr, tmp, head, entry) {
		if (ses_ptr->sid == sid) {
			list_del(&ses_ptr->entry);
			mutex_destroy(&ses_ptr->sem);
			kfree(ses_ptr);
			break;
		}
	}

	if (unlikely(!ses_ptr)) {
		dbgp(2, "Session with sid=0x%08X not found!", sid);
		ret = -EINVAL;
	}
	mutex_unlock(&fcr->sem);

	return ret;
}

static inline void crypto_put_session(struct crypto_session *ses_ptr)
{
	mutex_unlock(&ses_ptr->sem);
}

/* Look up session by session ID. The returned session is locked. */
struct crypto_session *crypto_get_session_by_sid(struct fcrypt *fcr,
						 u32 sid)
{
	struct crypto_session *ses_ptr, *retval = NULL;

	if (unlikely(!fcr))
		return NULL;

	mutex_lock(&fcr->sem);
	list_for_each_entry(ses_ptr, &fcr->list, entry) {
		if (ses_ptr->sid == sid) {
			mutex_lock(&ses_ptr->sem);
			retval = ses_ptr;
			break;
		}
	}
	mutex_unlock(&fcr->sem);

	return retval;
}

static int fill_kcop_from_cop(struct kernel_crypt_op *kcop, struct fcrypt *fcr)
{
	struct crypt_op *cop = &kcop->cop;
	struct crypto_session *ses_ptr;
	int rc;

	/* this also enters ses_ptr->sem */
	ses_ptr = crypto_get_session_by_sid(fcr, cop->ses);
	if (unlikely(!ses_ptr)) {
		dbgp(2, "invalid session ID=0x%08X", cop->ses);
		return -EINVAL;
	}

	if (unlikely(cop->iv && cop->ivlen < ses_ptr->cdata.ivsize)) {
		dbgp(2, "invalid ivlen = %d", cop->ivlen);
		return -EINVAL;
	}

	kcop->ivlen = cop->iv ? ses_ptr->cdata.ivsize : 0;
	if (kcop->ivlen > MAX_IV_SIZE) {
		dbgp(2, "ivlen = %d is too large", kcop->ivlen);
		return -EINVAL;
	}
	kcop->param_len = cop->param_len;

	crypto_put_session(ses_ptr);

	if (cop->iv) {
		rc = copy_from_user(kcop->iv, cop->iv, kcop->ivlen);
		if (unlikely(rc)) {
			dbgp(2, "error copying IV (%d bytes), returned %d",
			     kcop->ivlen, rc);
			return -EFAULT;
		}
	}
	if (cop->param) {
		kcop->param = kzalloc(kcop->param_len, GFP_KERNEL);
		if (!kcop->param) {
			dbgp(2, "error allocating %d for param",
			     kcop->param_len);
			return -ENOMEM;
		}
		rc = copy_from_user(kcop->param, cop->param, kcop->param_len);
		if (unlikely(rc)) {
			dbgp(2, "error copying param (%d bytes), returned %d",
					kcop->param_len, rc);
			return -EFAULT;
		}
	} else {
		kcop->param = NULL;
		kcop->param_len = 0;
	}

	return 0;
}

static int fill_cop_from_kcop(struct kernel_crypt_op *kcop, struct fcrypt *fcr)
{
	int ret;

	if (kcop->ivlen) {
		ret = copy_to_user(kcop->cop.iv,
				kcop->iv, kcop->ivlen);
		if (unlikely(ret))
			return -EFAULT;
	}
	return 0;
}

static int kcop_from_user(struct kernel_crypt_op *kcop,
			  struct fcrypt *fcr, void __user *arg)
{
	if (unlikely(copy_from_user(&kcop->cop, arg, sizeof(kcop->cop))))
		return -EFAULT;

	if (kcop->cop.num_src_bufs > MAX_CRYPTO_BUFFERS ||
	    kcop->cop.num_dst_bufs > MAX_CRYPTO_BUFFERS) {
		dbgp(2, "too many buffers, src: %d, dst: %d\n",
		     kcop->cop.num_src_bufs, kcop->cop.num_dst_bufs);
		return -EINVAL;
	}
	return fill_kcop_from_cop(kcop, fcr);
}

static int kcop_to_user(struct kernel_crypt_op *kcop,
			struct fcrypt *fcr, void __user *arg)
{
	int ret;

	ret = fill_cop_from_kcop(kcop, fcr);
	kfree(kcop->param);

	if (unlikely(ret)) {
		dbgp(2, "Error in fill_cop_from_kcop");
		return ret;
	}

	if (unlikely(copy_to_user(arg, &kcop->cop, sizeof(kcop->cop)))) {
		dbgp(2, "Cannot copy to userspace");
		return -EFAULT;
	}
	return 0;
}

static int hw_restriction_check(u32 nbufs, struct crypt_mem *buf,
				struct crypto_session *ses_ptr)
{
	int i = 0;
	u32 length = 0, dma_length = 0;
	int rc = 0;

	for (i = 0; i < nbufs; i++) {
		length = buf[i].length;
		if (!(length % DMA_BLOCK_MODE_SIZE))
			dma_length = length / DMA_BLOCK_MODE_SIZE;
		else
			dma_length = length;
		if (dma_length >> 18) {
			/* length too large for byte mode */
			rc = -EINVAL;
			dbgp(2, "size too large: %d\n", dma_length);
			return rc;
		}

		if (length % ses_ptr->cdata.blocksize) {
			if (ses_ptr->cdata.cipher == CRYPTO_OP_AES_CTR ||
					ses_ptr->cdata.cipher == CRYPTO_OP_SM4_CTR) {
				if (i != (nbufs - 1)) {
					dbgp(2, "unaligned buffer size: %d\n",
					     length);
					return -EINVAL;
				}
			} else {
				dbgp(2, "unaligned buffer size: %d\n",
				     length);
				return -EINVAL;
			}
		}
	}

	return rc;
}

/* Increment 128-bit counter */
static void ctr_update_iv(unsigned char *val, unsigned int value)
{
	int i;

	while (value > 0) {
		for (i = 15; i >= 0; i--) {
			val[i] = (val[i] + 1) & 0xff;
			if (val[i])
				break;
		}
		value--;
	}
}

int __crypto_run_physical(struct crypto_session *ses_ptr,
			  struct kernel_crypt_op *kcop)
{
	int i = 0;
	dma_addr_t dsc_addr = 0;
	dma_addr_t dma_iv_addr = 0;
	dma_addr_t cleanup_addr = 0;
	struct dma_dsc *dsc = NULL;
	u8 *iv = NULL;
	u8 *key_clean = NULL;
	struct device *dev = NULL;
	int s = 0;
	u32 nbufs = 0;
	int rc = 0;
	int block_mode = 0;
	u32 src_bufsz = 0;
	int err = 0;
	const u32 DMA_BUF_SIZE = kcop->ivlen > 16 ?
				 DMA_KEY_IV_BUF_SIZE_64B : DMA_KEY_IV_BUF_SIZE;
	u8 begin = 0;

	if (unlikely(!ses_ptr || !kcop)) {
		dbgp(2, "no ses_ptr or no kcop\n");
		return -EINVAL;
	}

	dev = crypto_dd->dev;

	/* data sanity check for HW restrictions */
	if (kcop->cop.num_src_bufs != kcop->cop.num_dst_bufs) {
		dbgp(2, "incompatible number of buffers, src: %d, dst: %d\n",
		     kcop->cop.num_src_bufs, kcop->cop.num_dst_bufs);
		return -EINVAL;
	}

	nbufs = kcop->cop.num_src_bufs;
	if (nbufs > MAX_CRYPTO_BUFFERS) {
		dbgp(2, "too many buffers: %d\n", nbufs);
		return -EINVAL;
	}

	for (i = 0; i < nbufs; i++) {
		if (kcop->cop.src[i].length != kcop->cop.dst[i].length) {
			dbgp(2, "incompatible buffer size\n");
			dbgp(2, "buffer: %d, src: %d, dst: %d\n",
			     i, kcop->cop.src[i].length,
			     kcop->cop.src[i].length);
			return -EINVAL;
		}
	}
	if (hw_restriction_check(nbufs, kcop->cop.src, ses_ptr)) {
		dbgp(2, "checked fail in src\n");
		return -EINVAL;
	}
	if (hw_restriction_check(nbufs, kcop->cop.dst, ses_ptr)) {
		dbgp(2, "checked fail in dst\n");
		return -EINVAL;
	}
	for (i = 0; i < nbufs; i++)
		src_bufsz += kcop->cop.src[i].length;

	/* extra three descriptor for key and IV and cleanup */
	dsc = dma_alloc_coherent(dev, (nbufs + 3) * sizeof(struct dma_dsc),
				 &dsc_addr, GFP_KERNEL | GFP_DMA);
	if (!dsc) {
		dbgp(2, "cannot allocate dsc\n");
		rc = -ENOMEM;
		goto error;
	}

	dsc[0].src_addr = (u32)(0xffffff00 | ses_ptr->cdata.kte);
	dsc[0].tgt_addr = 0;
	dsc[0].dsc_cfg.d32 = 0;
	/* HW bug, key length needs to stay at 16 */
	dsc[0].dsc_cfg.b.length = ses_ptr->cdata.keylen <= 16 ?
				  16 : ses_ptr->cdata.keylen;
	dsc[0].dsc_cfg.b.mode = MODE_KEY;
	dsc[0].dsc_cfg.b.eoc = 0;
	dsc[0].dsc_cfg.b.owner = 1;
	s++;

	if (kcop->ivlen) {
		iv = dma_alloc_coherent(dev, kcop->ivlen,
				&dma_iv_addr, GFP_KERNEL | GFP_DMA);
		if (!iv) {
			dbgp(2, "cannot allocate iv\n");
			rc = -ENOMEM;
			goto error;
		}
		memcpy(iv, kcop->iv, kcop->ivlen);
		dsc[1].src_addr = (u32)dma_iv_addr;
		dsc[1].tgt_addr = 32;
		dsc[1].dsc_cfg.d32 = 0;
		/* HW bug, iv length needs to stay at 16 */
		dsc[1].dsc_cfg.b.length = kcop->ivlen <= 16 ? 16 : kcop->ivlen;
		dsc[1].dsc_cfg.b.mode = MODE_KEY;
		dsc[1].dsc_cfg.b.eoc = 0;
		dsc[1].dsc_cfg.b.owner = 1;
		s++;
	}

	/* Do nothing for IV: We do not know IV in encrypt */

	if (ses_ptr->cdata.cipher == CRYPTO_OP_S17_ECB ||
	    ses_ptr->cdata.cipher == CRYPTO_OP_S17_CBC ||
	    ses_ptr->cdata.cipher == CRYPTO_OP_S17_CTR)
		begin = 1;
	else
		begin = 0;

	for (i = 0; i < nbufs; i++) {
		u32 length = kcop->cop.src[i].length;

		if (!(length % DMA_BLOCK_MODE_SIZE))
			block_mode = 1;
		else
			block_mode = 0;

		dsc[s + i].src_addr = (uintptr_t)kcop->cop.src[i].addr;
		dsc[s + i].tgt_addr = (uintptr_t)kcop->cop.dst[i].addr;
		dsc[s + i].dsc_cfg.d32 = 0;
		dsc[s + i].dsc_cfg.b.enc_sha_only = (kcop->cop.op ==
						     CRYPTO_OP_ENCRYPT);
		dsc[s + i].dsc_cfg.b.block = block_mode;
		if (block_mode)
			dsc[s + i].dsc_cfg.b.length =
				length / DMA_BLOCK_MODE_SIZE;
		else
			dsc[s + i].dsc_cfg.b.length = length;
		dsc[s + i].dsc_cfg.b.op_mode = ses_ptr->cdata.op_mode;
		dsc[s + i].dsc_cfg.b.mode = ses_ptr->cdata.crypt_mode;
		dsc[s + i].dsc_cfg.b.begin = begin;
		dsc[s + i].dsc_cfg.b.eoc = 0;
		dsc[s + i].dsc_cfg.b.owner = 1;

		begin = 0;
	}
	key_clean = dma_alloc_coherent(dev, DMA_BUF_SIZE, &cleanup_addr,
				       GFP_KERNEL | GFP_DMA);
	if (!key_clean) {
		dbgp(2, "cannot allocate key_clean\n");
		rc = -ENOMEM;
		goto error;
	}
	memset(key_clean, 0, DMA_BUF_SIZE);
	dsc[s + i].src_addr = cleanup_addr;
	dsc[s + i].tgt_addr = 0;
	dsc[s + i].dsc_cfg.d32 = 0;
	dsc[s + i].dsc_cfg.b.length = DMA_BUF_SIZE;
	dsc[s + i].dsc_cfg.b.mode = MODE_KEY;
	dsc[s + i].dsc_cfg.b.eoc = 1;
	dsc[s + i].dsc_cfg.b.owner = 1;

	aml_dma_debug(dsc, s + i + 1, __func__,
		      crypto_dd->thread, crypto_dd->status);

	mutex_lock(&crypto_dd->lock);
	crypto_dd->dma_busy = 1;
	crypto_dd->processing = current;
	mutex_unlock(&crypto_dd->lock);

#if !USE_BUSY_POLLING
	set_current_state(TASK_INTERRUPTIBLE);
#endif

	aml_write_crypto_reg(crypto_dd->thread, (uintptr_t)dsc_addr | 2);

#if USE_BUSY_POLLING
	while ((err = aml_read_crypto_reg(crypto_dd->status)) == 0)
		;
	aml_write_crypto_reg(crypto_dd->status, 0xf);
#else
	schedule();
	err = crypto_dd->err;
#endif
	if (err & DMA_STATUS_KEY_ERROR) {
		rc = -EACCES;
		goto error;
	}

	if (ses_ptr->cdata.op_mode == OP_MODE_CBC) {
		/* Do nothing for IV: We do not know IV in decrypt */
	} else if (ses_ptr->cdata.op_mode == OP_MODE_CTR) {
		u32 dma_nblock =
			(src_bufsz + ses_ptr->cdata.blocksize - 1)
			/ ses_ptr->cdata.blocksize;
		ctr_update_iv(kcop->iv, dma_nblock);
	}

error:
	mutex_lock(&crypto_dd->lock);
	crypto_dd->dma_busy = 0;
	mutex_unlock(&crypto_dd->lock);

	if (iv)
		dma_free_coherent(dev, kcop->ivlen, iv, dma_iv_addr);
	if (dsc)
		dma_free_coherent(dev, (nbufs + 3) * sizeof(struct dma_dsc),
				  dsc, dsc_addr);
	if (key_clean)
		dma_free_coherent(dev, DMA_BUF_SIZE,
				  key_clean, cleanup_addr);
	return rc;
}

static int __copy_buffers_in(struct crypt_mem **src, u32 total, u32 *offset,
			     u8 *buf, int buflen)
{
	int rc = 0;
	int count, lvl = 0;

	while (total && buflen) {
		count = min((*src)->length - *offset, total);
		count = min(count, buflen);

		if (!count)
			return lvl;

		rc = copy_from_user(buf + lvl, (*src)->addr + *offset, count);
		if (unlikely(rc)) {
			dbgp(2, "error copying src (%d bytes)\n", count);
			dbgp(2, "copy_from_user returned %d", rc);
			return -1;
		}

		lvl += count;
		buflen -= count;
		*offset += count;
		total -= count;

		if (*offset == (*src)->length) {
			(*src)++;
			*offset = 0;
		}
	}

	return lvl;
}

static int __copy_buffers_out(struct crypt_mem **dst, u32 total,
			      u32 *offset, u8 *buf)
{
	int rc = 0;
	int count, lvl = 0;

	while (total) {
		count = min((*dst)->length - *offset, total);

		if (!count)
			return lvl;

		rc = copy_to_user((*dst)->addr + *offset, buf + lvl, count);
		if (unlikely(rc)) {
			dbgp(2, "error copying src (%d bytes)\n", count);
			dbgp(2, "copy_from_user returned %d\n", rc);
			return -1;
		}

		lvl += count;
		*offset += count;
		total -= count;

		if (*offset == (*dst)->length) {
			(*dst)++;
			*offset = 0;
		}
	}

	return lvl;
}

int __crypto_run_virt_to_phys(struct crypto_session *ses_ptr,
			      struct kernel_crypt_op *kcop)
{
	int i = 0;
	dma_addr_t dsc_addr = 0;
	dma_addr_t dma_iv_addr = 0;
	dma_addr_t dma_buf = 0;
	dma_addr_t cleanup_addr = 0;
	struct dma_dsc *dsc = NULL;
	u8 *iv = NULL;
	u8 *key_clean = NULL;
	struct device *dev = NULL;
	int s = 0;
	u32 nbufs = 0;
	int rc = 0;
	int block_mode = 0;
	u8 *tmp_buf = NULL, *tmp_buf2 = NULL;
	struct crypt_mem *src = kcop->cop.src;
	u32 length = 0;
	u32 offset = 0;
	int count = 0;
	u32 src_bufsz = 0;
	int err = 0;
	const u32 DMA_BUF_SIZE = kcop->ivlen > 16 ?
				 DMA_KEY_IV_BUF_SIZE_64B : DMA_KEY_IV_BUF_SIZE;
	u8 begin = 0;

	if (unlikely(!ses_ptr || !kcop)) {
		dbgp(2, "no ses_ptr or no kcop\n");
		return -EINVAL;
	}

	dev = crypto_dd->dev;

	/* data sanity check for HW restrictions */
	nbufs = kcop->cop.num_dst_bufs;
	if (nbufs > MAX_CRYPTO_BUFFERS) {
		dbgp(2, "too many buffers: %d\n", nbufs);
		return -EINVAL;
	}

	for (i = 0; i < nbufs; i++)
		src_bufsz += kcop->cop.src[i].length;
	if (hw_restriction_check(nbufs, kcop->cop.dst, ses_ptr)) {
		dbgp(2, "checked fail in dst\n");
		return -EINVAL;
	}

	/* three descriptor for key, IV and data */
	dsc = dma_alloc_coherent(dev, 3 * sizeof(struct dma_dsc),
				 &dsc_addr, GFP_KERNEL | GFP_DMA);
	if (!dsc) {
		dbgp(2, "cannot allocate dsc\n");
		rc = -ENOMEM;
		goto error;
	}

	dsc[0].src_addr = (u32)(0xffffff00 | ses_ptr->cdata.kte);
	dsc[0].tgt_addr = 0;
	dsc[0].dsc_cfg.d32 = 0;
	/* HW bug, key length needs to stay at 16 */
	dsc[0].dsc_cfg.b.length = ses_ptr->cdata.keylen <= 16 ?
				  16 : ses_ptr->cdata.keylen;
	dsc[0].dsc_cfg.b.mode = MODE_KEY;
	dsc[0].dsc_cfg.b.eoc = 0;
	dsc[0].dsc_cfg.b.owner = 1;
	s++;

	if (kcop->ivlen) {
		iv = dma_alloc_coherent(dev, kcop->ivlen,
				&dma_iv_addr, GFP_KERNEL | GFP_DMA);
		if (!iv) {
			dbgp(2, "cannot allocate iv\n");
			rc = -ENOMEM;
			goto error;
		}
		memcpy(iv, kcop->iv, kcop->ivlen);
		dsc[1].src_addr = (u32)dma_iv_addr;
		dsc[1].tgt_addr = 32;
		dsc[1].dsc_cfg.d32 = 0;
		/* HW bug, iv length needs to stay at 16 */
		dsc[1].dsc_cfg.b.length = kcop->ivlen <= 16 ? 16 : kcop->ivlen;
		dsc[1].dsc_cfg.b.mode = MODE_KEY;
		dsc[1].dsc_cfg.b.eoc = 0;
		dsc[1].dsc_cfg.b.owner = 1;
		s++;
	}

	if (ses_ptr->cdata.op_mode == OP_MODE_CBC &&
	    kcop->cop.op != CRYPTO_OP_ENCRYPT) {
		uintptr_t s_end = (uintptr_t)kcop->cop.src[nbufs - 1].addr +
			      kcop->cop.src[nbufs - 1].length;
		rc = copy_from_user(kcop->iv,
			       (void *)(s_end - ses_ptr->cdata.blocksize),
			       ses_ptr->cdata.blocksize);
		if (unlikely(rc)) {
			dbgp(2, "failed to get next IV\n");
			rc = -EPERM;
			goto error;
		}
	}

	mutex_lock(&crypto_dd->lock);
	crypto_dd->dma_busy = 1;
	crypto_dd->processing = current;
	mutex_unlock(&crypto_dd->lock);

	if (ses_ptr->cdata.cipher == CRYPTO_OP_S17_ECB ||
	    ses_ptr->cdata.cipher == CRYPTO_OP_S17_CBC ||
	    ses_ptr->cdata.cipher == CRYPTO_OP_S17_CTR)
		begin = 1;
	else
		begin = 0;

	for (i = 0; i < nbufs; i++) {
		length = kcop->cop.dst[i].length;
		tmp_buf2 = krealloc(tmp_buf, length, GFP_KERNEL | GFP_DMA);
		if (!tmp_buf2) {
			dbgp(2, "cannot allocate memory, size: %d\n", length);
			rc = -ENOMEM;
			goto error;
		}
		tmp_buf = tmp_buf2;
		count = __copy_buffers_in(&src, length, &offset,
					  tmp_buf, length);
		if (unlikely(count != length)) {
			dbgp(2, "incompatible num %d %d read\n", count, length);
			goto error;
		}

		if (!(length % DMA_BLOCK_MODE_SIZE))
			block_mode = 1;
		else
			block_mode = 0;

		dma_buf = dma_map_single(dev, tmp_buf, length, DMA_TO_DEVICE);

		dsc[s].src_addr = dma_buf;
		dsc[s].tgt_addr = (uintptr_t)kcop->cop.dst[i].addr;
		dsc[s].dsc_cfg.d32 = 0;
		dsc[s].dsc_cfg.b.enc_sha_only = (kcop->cop.op
						 == CRYPTO_OP_ENCRYPT);
		dsc[s].dsc_cfg.b.block = block_mode;
		if (block_mode)
			dsc[s].dsc_cfg.b.length =
				length / DMA_BLOCK_MODE_SIZE;
		else
			dsc[s].dsc_cfg.b.length = length;
		dsc[s].dsc_cfg.b.op_mode = ses_ptr->cdata.op_mode;
		dsc[s].dsc_cfg.b.mode = ses_ptr->cdata.crypt_mode;
		dsc[s].dsc_cfg.b.begin = begin;
		dsc[s].dsc_cfg.b.eoc = 1;
		dsc[s].dsc_cfg.b.owner = 1;

		begin = 0;

#if !USE_BUSY_POLLING
		set_current_state(TASK_INTERRUPTIBLE);
#endif

		aml_dma_debug(dsc, s + 1, __func__,
			      crypto_dd->thread, crypto_dd->status);
		aml_write_crypto_reg(crypto_dd->thread,
				    (uintptr_t)dsc_addr | 2);

#if USE_BUSY_POLLING
		while ((err = aml_read_crypto_reg(crypto_dd->status)) == 0)
			;
		aml_write_crypto_reg(crypto_dd->status, 0xf);
#else
		schedule();
		err = crypto_dd->err;
#endif
		dma_unmap_single(dev, dma_buf, length, DMA_TO_DEVICE);
		if (err & DMA_STATUS_KEY_ERROR) {
			rc = -EACCES;
			goto error;
		}
		s = 0;
	}

	key_clean = dma_alloc_coherent(dev, DMA_BUF_SIZE, &cleanup_addr,
			GFP_KERNEL | GFP_DMA);
	if (!key_clean) {
		dbgp(2, "cannot allocate key_clean\n");
		rc = -ENOMEM;
		goto error;
	}
	memset(key_clean, 0, DMA_BUF_SIZE);
	dsc[0].src_addr = cleanup_addr;
	dsc[0].tgt_addr = 0;
	dsc[0].dsc_cfg.d32 = 0;
	dsc[0].dsc_cfg.b.length = DMA_BUF_SIZE;
	dsc[0].dsc_cfg.b.mode = MODE_KEY;
	dsc[0].dsc_cfg.b.eoc = 1;
	dsc[0].dsc_cfg.b.owner = 1;

	aml_dma_debug(dsc, 1, __func__, crypto_dd->thread, crypto_dd->status);
	aml_write_crypto_reg(crypto_dd->thread, (uintptr_t)dsc_addr | 2);
#if USE_BUSY_POLLING
	while ((err = aml_read_crypto_reg(crypto_dd->status)) == 0)
		;
	aml_write_crypto_reg(crypto_dd->status, 0xf);
#else
	schedule();
	err = crypto_dd->err;
#endif
	if (err & DMA_STATUS_KEY_ERROR) {
		rc = -EACCES;
		goto error;
	}

	if (ses_ptr->cdata.op_mode == OP_MODE_CBC) {
		/* Do nothing for IV: We do not know IV in decrypt */
	} else if (ses_ptr->cdata.op_mode == OP_MODE_CTR) {
		u32 dma_nblock =
			(src_bufsz + ses_ptr->cdata.blocksize - 1)
			/ ses_ptr->cdata.blocksize;
		ctr_update_iv(kcop->iv, dma_nblock);
	}

error:

	mutex_lock(&crypto_dd->lock);
	crypto_dd->dma_busy = 0;
	mutex_unlock(&crypto_dd->lock);

	if (iv)
		dma_free_coherent(dev, kcop->ivlen, iv, dma_iv_addr);
	if (dsc)
		dma_free_coherent(dev, 3 * sizeof(struct dma_dsc),
				  dsc, dsc_addr);
	if (key_clean)
		dma_free_coherent(dev, DMA_BUF_SIZE,
				  key_clean, cleanup_addr);
	kfree(tmp_buf);

	return rc;
}

int __crypto_run_phys_to_virt(struct crypto_session *ses_ptr,
			      struct kernel_crypt_op *kcop)
{
	int i = 0;
	dma_addr_t dsc_addr = 0;
	dma_addr_t dma_iv_addr = 0;
	dma_addr_t dma_buf = 0;
	dma_addr_t cleanup_addr = 0;
	struct dma_dsc *dsc = NULL;
	u8 *iv = NULL;
	u8 *key_clean = NULL;
	struct device *dev = NULL;
	int s = 0;
	u32 nbufs = 0;
	int rc = 0;
	int block_mode = 0;
	u8 *tmp_buf = NULL, *tmp_buf2 = NULL;
	u32 length = 0;
	u32 offset_dst = 0;
	struct crypt_mem *dst = kcop->cop.dst;
	int count_dst = 0;
	u32 src_bufsz = 0;
	int err = 0;
	const u32 DMA_BUF_SIZE = kcop->ivlen > 16 ?
				 DMA_KEY_IV_BUF_SIZE_64B : DMA_KEY_IV_BUF_SIZE;
	u8 begin = 0;

	if (unlikely(!ses_ptr || !kcop)) {
		dbgp(2, "no ses_ptr or no kcop\n");
		return -EINVAL;
	}

	dev = crypto_dd->dev;

	/* data sanity check for HW restrictions */
	nbufs = kcop->cop.num_src_bufs;
	if (nbufs > MAX_CRYPTO_BUFFERS) {
		dbgp(2, "too many buffers: %d\n", nbufs);
		return -EINVAL;
	}

	if (hw_restriction_check(nbufs, kcop->cop.src, ses_ptr)) {
		dbgp(2, "checked fail in dst\n");
		return -EINVAL;
	}

	for (i = 0; i < nbufs; i++)
		src_bufsz += kcop->cop.src[i].length;

	/* three descriptor for key, IV and data */
	dsc = dma_alloc_coherent(dev, 3 * sizeof(struct dma_dsc),
				 &dsc_addr, GFP_KERNEL | GFP_DMA);
	if (!dsc) {
		dbgp(2, "cannot allocate dsc\n");
		rc = -ENOMEM;
		goto error;
	}

	dsc[0].src_addr = (u32)(0xffffff00 | ses_ptr->cdata.kte);
	dsc[0].tgt_addr = 0;
	dsc[0].dsc_cfg.d32 = 0;
	/* HW bug, key length needs to stay at 16 */
	dsc[0].dsc_cfg.b.length = ses_ptr->cdata.keylen <= 16 ?
				  16 : ses_ptr->cdata.keylen;
	dsc[0].dsc_cfg.b.mode = MODE_KEY;
	dsc[0].dsc_cfg.b.eoc = 0;
	dsc[0].dsc_cfg.b.owner = 1;
	s++;

	if (kcop->ivlen) {
		iv = dma_alloc_coherent(dev, kcop->ivlen,
				&dma_iv_addr, GFP_KERNEL | GFP_DMA);
		if (!iv) {
			dbgp(2, "cannot allocate iv\n");
			rc = -ENOMEM;
			goto error;
		}
		memcpy(iv, kcop->iv, kcop->ivlen);
		dsc[1].src_addr = (u32)dma_iv_addr;
		dsc[1].tgt_addr = 32;
		dsc[1].dsc_cfg.d32 = 0;
		/* HW bug, iv length needs to stay at 16 */
		dsc[1].dsc_cfg.b.length = kcop->ivlen <= 16 ? 16 : kcop->ivlen;
		dsc[1].dsc_cfg.b.mode = MODE_KEY;
		dsc[1].dsc_cfg.b.eoc = 0;
		dsc[1].dsc_cfg.b.owner = 1;
		s++;
	}

	/* Do nothing for IV: We do not know IV in encrypt */

	mutex_lock(&crypto_dd->lock);
	crypto_dd->dma_busy = 1;
	crypto_dd->processing = current;
	mutex_unlock(&crypto_dd->lock);

	if (ses_ptr->cdata.cipher == CRYPTO_OP_S17_ECB ||
	    ses_ptr->cdata.cipher == CRYPTO_OP_S17_CBC ||
	    ses_ptr->cdata.cipher == CRYPTO_OP_S17_CTR)
		begin = 1;
	else
		begin = 0;

	for (i = 0; i < nbufs; i++) {
		length = kcop->cop.src[i].length;
		tmp_buf2 = krealloc(tmp_buf, length, GFP_KERNEL | GFP_DMA);
		if (!tmp_buf2) {
			dbgp(2, "cannot allocate memory, size: %d\n", length);
			rc = -ENOMEM;
			goto error;
		}
		tmp_buf = tmp_buf2;

		if (!(length % DMA_BLOCK_MODE_SIZE))
			block_mode = 1;
		else
			block_mode = 0;

		dma_buf = dma_map_single(dev, tmp_buf, length, DMA_FROM_DEVICE);

		dsc[s].src_addr = (uintptr_t)kcop->cop.src[i].addr;
		dsc[s].tgt_addr = dma_buf;
		dsc[s].dsc_cfg.d32 = 0;
		dsc[s].dsc_cfg.b.enc_sha_only = (kcop->cop.op
						 == CRYPTO_OP_ENCRYPT);
		dsc[s].dsc_cfg.b.block = block_mode;
		if (block_mode)
			dsc[s].dsc_cfg.b.length =
				length / DMA_BLOCK_MODE_SIZE;
		else
			dsc[s].dsc_cfg.b.length = length;
		dsc[s].dsc_cfg.b.op_mode = ses_ptr->cdata.op_mode;
		dsc[s].dsc_cfg.b.mode = ses_ptr->cdata.crypt_mode;
		dsc[s].dsc_cfg.b.begin = begin;
		dsc[s].dsc_cfg.b.eoc = 1;
		dsc[s].dsc_cfg.b.owner = 1;

		begin = 0;

#if !USE_BUSY_POLLING
		set_current_state(TASK_INTERRUPTIBLE);
#endif

		aml_dma_debug(dsc, s + 1, __func__,
			      crypto_dd->thread, crypto_dd->status);
		aml_write_crypto_reg(crypto_dd->thread,
				    (uintptr_t)dsc_addr | 2);

#if USE_BUSY_POLLING
		while ((err = aml_read_crypto_reg(crypto_dd->status)) == 0)
			;
		aml_write_crypto_reg(crypto_dd->status, 0xf);
#else
		schedule();
		err = crypto_dd->err;
#endif
		dma_unmap_single(dev, dma_buf, length, DMA_FROM_DEVICE);
		if (err & DMA_STATUS_KEY_ERROR) {
			rc = -EACCES;
			goto error;
		}
		count_dst = __copy_buffers_out(&dst, length,
					       &offset_dst, tmp_buf);
		if (unlikely(count_dst != length)) {
			dbgp(2, "incorrect number of data, c = %d, dst = %d\n",
			     length, count_dst);
			goto error;
		}
		s = 0;
	}

	key_clean = dma_alloc_coherent(dev, DMA_BUF_SIZE, &cleanup_addr,
			GFP_KERNEL | GFP_DMA);
	if (!key_clean) {
		dbgp(2, "cannot allocate key_clean\n");
		rc = -ENOMEM;
		goto error;
	}
	memset(key_clean, 0, DMA_BUF_SIZE);
	dsc[0].src_addr = cleanup_addr;
	dsc[0].tgt_addr = 0;
	dsc[0].dsc_cfg.d32 = 0;
	dsc[0].dsc_cfg.b.length = DMA_BUF_SIZE;
	dsc[0].dsc_cfg.b.mode = MODE_KEY;
	dsc[0].dsc_cfg.b.eoc = 1;
	dsc[0].dsc_cfg.b.owner = 1;

	aml_dma_debug(dsc, 1, __func__, crypto_dd->thread, crypto_dd->status);
	aml_write_crypto_reg(crypto_dd->thread, (uintptr_t)dsc_addr | 2);
#if USE_BUSY_POLLING
	while ((err = aml_read_crypto_reg(crypto_dd->status)) == 0)
		;
	aml_write_crypto_reg(crypto_dd->status, 0xf);
#else
	schedule();
	err = crypto_dd->err;
#endif
	if (err & DMA_STATUS_KEY_ERROR) {
		rc = -EACCES;
		goto error;
	}

	if (ses_ptr->cdata.op_mode == OP_MODE_CBC) {
		if (kcop->cop.op == CRYPTO_OP_ENCRYPT) {
			memcpy(kcop->iv, tmp_buf + count_dst -
			       ses_ptr->cdata.blocksize,
			       ses_ptr->cdata.blocksize);
		}
	} else if (ses_ptr->cdata.op_mode == OP_MODE_CTR) {
		u32 dma_nblock =
			(src_bufsz + ses_ptr->cdata.blocksize - 1)
			/ ses_ptr->cdata.blocksize;
		ctr_update_iv(kcop->iv, dma_nblock);
	}

error:
	mutex_lock(&crypto_dd->lock);
	crypto_dd->dma_busy = 0;
	mutex_unlock(&crypto_dd->lock);

	if (iv)
		dma_free_coherent(dev, kcop->ivlen, iv, dma_iv_addr);
	if (dsc)
		dma_free_coherent(dev, 3 * sizeof(struct dma_dsc),
				  dsc, dsc_addr);
	if (key_clean)
		dma_free_coherent(dev, DMA_BUF_SIZE,
				  key_clean, cleanup_addr);
	kfree(tmp_buf);

	return rc;
}

int __crypto_run_virtual(struct crypto_session *ses_ptr,
			 struct kernel_crypt_op *kcop)
{
	int i = 0;
	dma_addr_t dsc_addr = 0;
	dma_addr_t dma_iv_addr = 0;
	dma_addr_t dma_buf = 0;
	dma_addr_t cleanup_addr = 0;
	struct dma_dsc *dsc = NULL;
	u8 *key_clean = NULL;
	u8 *iv = NULL;
	struct device *dev = NULL;
	int s = 0;
	u32 nsrc_bufs = 0;
	u32 ndst_bufs = 0;
	u32 total = 0;
	u32 src_bufsz = 0;
	u32 dst_bufsz = 0;
	u8 *tmp_buf = NULL;
	int rc = 0;
	u32 offset = 0, offset_dst = 0;
	struct crypt_mem *src = kcop->cop.src;
	struct crypt_mem *dst = kcop->cop.dst;
	int count = 0, count_dst = 0;
	int err = 0;
	const u32 DMA_BUF_SIZE = kcop->ivlen > 16 ?
				 DMA_KEY_IV_BUF_SIZE_64B : DMA_KEY_IV_BUF_SIZE;
	u8 begin = 0;

	if (unlikely(!ses_ptr || !kcop)) {
		dbgp(2, "no ses_ptr or no kcop\n");
		return -EINVAL;
	}

	dev = crypto_dd->dev;

	nsrc_bufs = kcop->cop.num_src_bufs;
	if (nsrc_bufs > MAX_CRYPTO_BUFFERS) {
		dbgp(2, "too many buffers: %d\n", nsrc_bufs);
		return -EINVAL;
	}

	for (i = 0; i < nsrc_bufs; i++)
		src_bufsz += kcop->cop.src[i].length;
	total = src_bufsz;

	ndst_bufs = kcop->cop.num_dst_bufs;
	if (ndst_bufs > MAX_CRYPTO_BUFFERS) {
		dbgp(2, "too many buffers: %d\n", ndst_bufs);
		return -EINVAL;
	}

	for (i = 0; i < ndst_bufs; i++)
		dst_bufsz += kcop->cop.dst[i].length;

	if (dst_bufsz < src_bufsz) {
		dbgp(2, "buffer size mismatch, src: %d, dst: %d\n",
		     src_bufsz, dst_bufsz);
		return -EINVAL;
	}

	tmp_buf = (void *)__get_free_pages(GFP_KERNEL, 0);
	if (!tmp_buf) {
		dbgp(2, "cannot allocate tmpbuf\n");
		rc = -ENOMEM;
		goto error;
	}
	dma_buf = dma_map_single(dev, tmp_buf, PAGE_SIZE, DMA_TO_DEVICE);

	/* three descriptor for key, IV and data */
	dsc = dma_alloc_coherent(dev, 3 * sizeof(struct dma_dsc),
				 &dsc_addr, GFP_KERNEL | GFP_DMA);
	if (!dsc) {
		dbgp(2, "cannot allocate dsc\n");
		rc = -ENOMEM;
		goto error;
	}

	dsc[0].src_addr = (u32)(0xffffff00 | ses_ptr->cdata.kte);
	dsc[0].tgt_addr = 0;
	dsc[0].dsc_cfg.d32 = 0;
	/* HW bug, key length needs to stay at 16 */
	dsc[0].dsc_cfg.b.length = ses_ptr->cdata.keylen <= 16 ?
				  16 : ses_ptr->cdata.keylen;
	dsc[0].dsc_cfg.b.mode = MODE_KEY;
	dsc[0].dsc_cfg.b.eoc = 0;
	dsc[0].dsc_cfg.b.owner = 1;
	s++;

	if (kcop->ivlen) {
		iv = dma_alloc_coherent(dev, kcop->ivlen,
				&dma_iv_addr, GFP_KERNEL | GFP_DMA);
		if (!iv) {
			dbgp(2, "cannot allocate iv\n");
			rc = -ENOMEM;
			goto error;
		}
		memcpy(iv, kcop->iv, kcop->ivlen);

		dsc[1].src_addr = (u32)dma_iv_addr;
		dsc[1].tgt_addr = 32;
		dsc[1].dsc_cfg.d32 = 0;
		/* HW bug, iv length needs to stay at 16 */
		dsc[1].dsc_cfg.b.length = kcop->ivlen <= 16 ? 16 : kcop->ivlen;
		dsc[1].dsc_cfg.b.mode = MODE_KEY;
		dsc[1].dsc_cfg.b.eoc = 0;
		dsc[1].dsc_cfg.b.owner = 1;
		s++;
	}

	if (ses_ptr->cdata.op_mode == OP_MODE_CBC &&
	    kcop->cop.op != CRYPTO_OP_ENCRYPT) {
		uintptr_t s_end = (uintptr_t)kcop->cop.src[nsrc_bufs - 1].addr +
			      kcop->cop.src[nsrc_bufs - 1].length;
		rc = copy_from_user(kcop->iv,
			       (void *)(s_end - ses_ptr->cdata.blocksize),
			       ses_ptr->cdata.blocksize);
		if (unlikely(rc)) {
			dbgp(2, "failed to get next IV\n");
			rc = -EPERM;
			goto error;
		}
	}

	mutex_lock(&crypto_dd->lock);
	crypto_dd->dma_busy = 1;
	crypto_dd->processing = current;
	mutex_unlock(&crypto_dd->lock);

	if (ses_ptr->cdata.cipher == CRYPTO_OP_S17_ECB ||
	    ses_ptr->cdata.cipher == CRYPTO_OP_S17_CBC ||
	    ses_ptr->cdata.cipher == CRYPTO_OP_S17_CTR)
		begin = 1;
	else
		begin = 0;

	while (total) {
		count = __copy_buffers_in(&src, total, &offset,
					  tmp_buf, PAGE_SIZE);
		if (count < 0)
			goto error;
		dma_sync_single_for_device(dev, dma_buf,
					   PAGE_SIZE, DMA_TO_DEVICE);

		dsc[s].src_addr = dma_buf;
		dsc[s].tgt_addr = dma_buf;
		dsc[s].dsc_cfg.d32 = 0;
		dsc[s].dsc_cfg.b.enc_sha_only = (kcop->cop.op
						 == CRYPTO_OP_ENCRYPT);
		dsc[s].dsc_cfg.b.length = count;
		dsc[s].dsc_cfg.b.op_mode = ses_ptr->cdata.op_mode;
		dsc[s].dsc_cfg.b.mode = ses_ptr->cdata.crypt_mode;
		dsc[s].dsc_cfg.b.begin = begin;
		dsc[s].dsc_cfg.b.eoc = 1;
		dsc[s].dsc_cfg.b.owner = 1;

		begin = 0;

#if !USE_BUSY_POLLING
		set_current_state(TASK_INTERRUPTIBLE);
#endif

		aml_dma_debug(dsc, s + 1, __func__,
			      crypto_dd->thread, crypto_dd->status);
		aml_write_crypto_reg(crypto_dd->thread,
				     (uintptr_t)dsc_addr | 2);

#if USE_BUSY_POLLING
		while ((err = aml_read_crypto_reg(crypto_dd->status)) == 0)
			;
		aml_write_crypto_reg(crypto_dd->status, 0xf);
#else
		schedule();
		err = crypto_dd->err;
#endif
		dma_sync_single_for_cpu(dev, dma_buf,
					PAGE_SIZE, DMA_FROM_DEVICE);
		if (err & DMA_STATUS_KEY_ERROR) {
			rc = -EACCES;
			goto error;
		}
		count_dst = __copy_buffers_out(&dst, count,
					       &offset_dst, tmp_buf);
		if (unlikely(count_dst != count)) {
			dbgp(2, "incorrect number of data, c = %d, dst = %d\n",
			     count, count_dst);
			goto error;
		}
		total -= count;
		s = 0;
	}

	key_clean = dma_alloc_coherent(dev, DMA_BUF_SIZE, &cleanup_addr,
			GFP_KERNEL | GFP_DMA);
	if (!key_clean) {
		dbgp(2, "cannot allocate key_clean\n");
		rc = -ENOMEM;
		goto error;
	}
	memset(key_clean, 0, DMA_BUF_SIZE);
	dsc[0].src_addr = cleanup_addr;
	dsc[0].tgt_addr = 0;
	dsc[0].dsc_cfg.d32 = 0;
	dsc[0].dsc_cfg.b.length = DMA_BUF_SIZE;
	dsc[0].dsc_cfg.b.mode = MODE_KEY;
	dsc[0].dsc_cfg.b.eoc = 1;
	dsc[0].dsc_cfg.b.owner = 1;

	aml_dma_debug(dsc, 1, __func__, crypto_dd->thread, crypto_dd->status);
	aml_write_crypto_reg(crypto_dd->thread, (uintptr_t)dsc_addr | 2);
#if USE_BUSY_POLLING
	while ((err = aml_read_crypto_reg(crypto_dd->status)) == 0)
		;
	aml_write_crypto_reg(crypto_dd->status, 0xf);
#else
	schedule();
	err = crypto_dd->err;
#endif
	if (err & DMA_STATUS_KEY_ERROR) {
		rc = -EACCES;
		goto error;
	}

	if (ses_ptr->cdata.op_mode == OP_MODE_CBC) {
		if (kcop->cop.op == CRYPTO_OP_ENCRYPT) {
			memcpy(kcop->iv, tmp_buf + count_dst -
			       ses_ptr->cdata.blocksize,
			       ses_ptr->cdata.blocksize);
		}
	} else if (ses_ptr->cdata.op_mode == OP_MODE_CTR) {
		u32 dma_nblock =
			(src_bufsz + ses_ptr->cdata.blocksize - 1)
			/ ses_ptr->cdata.blocksize;
		ctr_update_iv(kcop->iv, dma_nblock);
	}

error:
	mutex_lock(&crypto_dd->lock);
	crypto_dd->dma_busy = 0;
	mutex_unlock(&crypto_dd->lock);

	if (dma_buf)
		dma_unmap_single(dev, dma_buf, PAGE_SIZE, DMA_TO_DEVICE);
	if (iv)
		dma_free_coherent(dev, kcop->ivlen, iv, dma_iv_addr);
	if (dsc)
		dma_free_coherent(dev, 3 * sizeof(struct dma_dsc),
				  dsc, dsc_addr);
	if (key_clean)
		dma_free_coherent(dev, DMA_BUF_SIZE,
				  key_clean, cleanup_addr);
	if (tmp_buf)
		free_page((uintptr_t)tmp_buf);

	return rc;
}

int crypto_run(struct fcrypt *fcr, struct kernel_crypt_op *kcop)
{
	struct crypto_session *ses_ptr;
	struct crypt_op *cop = &kcop->cop;
	int ret = 0;

	if (unlikely(cop->op != CRYPTO_OP_ENCRYPT &&
		     cop->op != CRYPTO_OP_DECRYPT)) {
		dbgp(2, "invalid operation op=%u", cop->op);
		return -EINVAL;
	}

	if (crypto_dd->dma_busy)
		wait_event_interruptible(crypto_dd->waiter,
					 crypto_dd->dma_busy == 0);

	/* this also enters ses_ptr->sem */
	ses_ptr = crypto_get_session_by_sid(fcr, cop->ses);
	if (unlikely(!ses_ptr)) {
		dbgp(2, "invalid session ID=0x%08X", cop->ses);
		ret = -EINVAL;
		goto out;
	}

	if (ses_ptr->cdata.init != 0) {
		if (ses_ptr->cdata.cipher == CRYPTO_OP_S17_ECB ||
		    ses_ptr->cdata.cipher == CRYPTO_OP_S17_CBC ||
		    ses_ptr->cdata.cipher == CRYPTO_OP_S17_CTR) {
			if (kcop->param && kcop->param_len == 4) {
				u32 s17_cfg = 0;

				memcpy(&s17_cfg, kcop->param, kcop->param_len);
				s17_cfg = ((s17_cfg & 0x1fff) |
					    crypto_dd->thread << 13);
				if (call_smc(CRYPTO_OPERATION_CFG,
					     SET_S17_M2M_CFG, s17_cfg, 0)) {
					dbgp(0, "Not support s17 cfg from ree");
					ret = -EINVAL;
					goto out;
				}
			}
		}

		if (ses_ptr->cdata.cipher == CRYPTO_OP_AES_CTR ||
				ses_ptr->cdata.cipher == CRYPTO_OP_SM4_CTR) {
			/* aes-ctr and sm4-ctr only supports encrypt */
			kcop->cop.op = CRYPTO_OP_ENCRYPT;
		}
		if (ses_ptr->cdata.op_mode != OP_MODE_ECB && !kcop->cop.ivlen) {
			dbgp(2, "no iv for non-ecb\n");
			ret = -EINVAL;
			goto out_unlock;
		}

		if (kcop->cop.src_phys && kcop->cop.dst_phys)
			ret = __crypto_run_physical(ses_ptr, kcop);
		else if (!kcop->cop.src_phys && kcop->cop.dst_phys)
			ret = __crypto_run_virt_to_phys(ses_ptr, kcop);
		else if (kcop->cop.src_phys && !kcop->cop.dst_phys)
			ret = __crypto_run_phys_to_virt(ses_ptr, kcop);
		else
			ret = __crypto_run_virtual(ses_ptr, kcop);
	} else {
		ret = -EINVAL;
		goto out_unlock;
	}

out_unlock:
	crypto_put_session(ses_ptr);

out:
	wake_up_interruptible(&crypto_dd->waiter);
	return ret;
}

static int aml_crypto_dev_open(struct inode *inode, struct file *filp)
{
	struct crypt_priv *pcr;

	pcr = kzalloc(sizeof(*pcr), GFP_KERNEL);
	if (!pcr)
		return -ENOMEM;
	filp->private_data = pcr;

	mutex_init(&pcr->fcrypt.sem);

	INIT_LIST_HEAD(&pcr->fcrypt.list);
	return 0;
}

static int aml_crypto_dev_release(struct inode *inode, struct file *filp)
{
	struct crypt_priv *pcr = filp->private_data;

	if (!pcr)
		return 0;

	mutex_destroy(&pcr->fcrypt.sem);
	kfree(pcr);
	filp->private_data = NULL;

	return 0;
}

static long aml_crypto_dev_ioctl(struct file *filp,
				 unsigned int cmd, unsigned long arg_)
{
	void __user *arg = (void __user *)arg_;
	struct session_op sop;
	struct kernel_crypt_op kcop;
	struct crypt_priv *pcr = filp->private_data;
	struct fcrypt *fcr;
	u32 ses;
	int ret = 0;

	if (unlikely(!pcr)) {
		dbgp(2, "empty pcr\n");
		return -EINVAL;
	}

	fcr = &pcr->fcrypt;

	switch (cmd) {
	case CREATE_SESSION:
		if (unlikely(copy_from_user(&sop, arg, sizeof(sop))))
			return -EFAULT;

		ret = crypto_create_session(fcr, &sop);
		if (unlikely(ret))
			return ret;
		ret = copy_to_user(arg, &sop, sizeof(sop));
		if (unlikely(ret)) {
			crypto_finish_session(fcr, sop.ses);
			return -EFAULT;
		}
		return ret;
	case CLOSE_SESSION:
		ret = get_user(ses, (u32 __user *)arg);
		if (unlikely(ret))
			return ret;
		ret = crypto_finish_session(fcr, ses);
		return ret;

	case DO_CRYPTO:
		ret = kcop_from_user(&kcop, fcr, arg);
		if (unlikely(ret)) {
			dbgp(2, "Error copying from user");
			return ret;
		}

		ret = crypto_run(fcr, &kcop);
		if (unlikely(ret)) {
			dbgp(2, "Error in crypto_run");
			return ret;
		}

		return kcop_to_user(&kcop, fcr, arg);
	default:
		dbgp(3, "unknown cmd: %d\n", cmd);
		return -EINVAL;
	}
}

#ifdef CONFIG_COMPAT
static inline void compat_to_crypt_op(struct compat_crypt_op *compat,
				      struct crypt_op *cop)
{
	int i = 0;
#if TEST_PHYSICAL_PROCESS
	struct device *dev = NULL;
	int rc = 0;

	dev = crypto_dd->dev;
#endif

	cop->ses = compat->ses;
	cop->op = compat->op;
	cop->src_phys = compat->src_phys;
	cop->dst_phys = compat->dst_phys;
	cop->ivlen = compat->ivlen;
	cop->num_src_bufs = compat->num_src_bufs;
	cop->num_dst_bufs = compat->num_src_bufs;
	cop->param_len = compat->param_len;
	cop->reserved = 0;

	for (i = 0; i < MAX_CRYPTO_BUFFERS; i++) {
		if (cop->src_phys) {
#if TEST_PHYSICAL_PROCESS
			g_src[i] = dma_alloc_coherent(dev,
					compat->src[i].length,
					&g_src_dma[i],
					GFP_KERNEL | GFP_DMA);
			g_tmp_src_addr[i] = compat_ptr(compat->src[i].addr);
			cop->src[i].addr = (u8 *)(uintptr_t)g_src_dma[i];
			cop->src[i].length = compat->src[i].length;
			rc = copy_from_user(g_src[i],
					    compat_ptr(compat->src[i].addr),
					    compat->src[i].length);
			if (unlikely(rc))
				dev_err(dev, "failed at %s %d\n",
					__func__, __LINE__);
#else
			cop->src[i].addr = (u8 *)(uintptr_t)compat->src[i].addr;
			cop->src[i].length = compat->src[i].length;
#endif
		} else {
			cop->src[i].addr = compat_ptr(compat->src[i].addr);
			cop->src[i].length = compat->src[i].length;
		}
		if (cop->dst_phys) {
#if TEST_PHYSICAL_PROCESS
			g_dst[i] = dma_alloc_coherent(dev,
						      compat->dst[i].length,
						      &g_dst_dma[i],
						      GFP_KERNEL | GFP_DMA);
			g_tmp_dst_addr[i] = compat_ptr(compat->dst[i].addr);
			cop->dst[i].addr = (u8 *)(uintptr_t)g_dst_dma[i];
			cop->dst[i].length = compat->dst[i].length;
			rc = copy_from_user(g_dst[i],
					    compat_ptr(compat->dst[i].addr),
					    compat->src[i].length);
			if (unlikely(rc))
				dev_err(dev, "failed at %s %d\n",
					__func__, __LINE__);
#else
			cop->dst[i].addr = (u8 *)(uintptr_t)compat->dst[i].addr;
			cop->dst[i].length = compat->dst[i].length;
#endif
		} else {
			cop->dst[i].addr = compat_ptr(compat->dst[i].addr);
			cop->dst[i].length = compat->dst[i].length;
		}
	}
	cop->iv  = compat_ptr(compat->iv);
	cop->param  = compat_ptr(compat->param);
}

static inline void crypt_op_to_compat(struct crypt_op *cop,
				      struct compat_crypt_op *compat)
{
	int i = 0;
#if TEST_PHYSICAL_PROCESS
	int rc = 0;
	struct device *dev = NULL;

	dev = crypto_dd->dev;
#endif

	compat->ses = cop->ses;
	compat->op = cop->op;
	compat->src_phys = cop->src_phys;
	compat->dst_phys = cop->dst_phys;
	compat->ivlen = cop->ivlen;
	compat->num_src_bufs = cop->num_src_bufs;
	compat->num_dst_bufs = cop->num_src_bufs;
	compat->param_len = cop->param_len;
	compat->reserved = 0;

	for (i = 0; i < MAX_CRYPTO_BUFFERS; i++) {
		if (cop->src_phys) {
#if TEST_PHYSICAL_PROCESS
			rc = copy_to_user(g_tmp_src_addr[i],
					  g_src[i], cop->src[i].length);
			if (unlikely(rc))
				dev_err(dev, "failed at %s %d\n",
					__func__, __LINE__);
			compat->src[i].addr = ptr_to_compat(g_tmp_src_addr[i]);
			compat->src[i].length = cop->src[i].length;
			dma_free_coherent(dev, cop->src[i].length,
					  g_src[i], g_src_dma[i]);
#else
			compat->src[i].addr = (uintptr_t)cop->src[i].addr;
			compat->src[i].length = cop->src[i].length;
#endif
		} else {
			compat->src[i].addr = ptr_to_compat(cop->src[i].addr);
			compat->src[i].length = cop->src[i].length;
		}
		if (cop->dst_phys) {
#if TEST_PHYSICAL_PROCESS
			rc = copy_to_user(g_tmp_dst_addr[i], g_dst[i],
					  cop->dst[i].length);
			if (unlikely(rc))
				dev_err(dev, "failed at %s %d\n",
					__func__, __LINE__);
			compat->dst[i].addr = ptr_to_compat(g_tmp_dst_addr[i]);
			compat->dst[i].length = cop->dst[i].length;
			dma_free_coherent(dev, cop->dst[i].length,
					  g_dst[i], g_dst_dma[i]);
#else
			compat->dst[i].addr = (uintptr_t)cop->dst[i].addr;
			compat->dst[i].length = cop->dst[i].length;
#endif
		} else {
			compat->dst[i].addr = ptr_to_compat(cop->dst[i].addr);
			compat->dst[i].length = cop->dst[i].length;
		}
	}
	compat->iv  = ptr_to_compat(cop->iv);
	compat->param  = ptr_to_compat(cop->param);
}

static int compat_kcop_from_user(struct kernel_crypt_op *kcop,
				 struct fcrypt *fcr, void __user *arg)
{
	struct compat_crypt_op compat_cop;

	if (unlikely(copy_from_user(&compat_cop, arg, sizeof(compat_cop))))
		return -EFAULT;
	compat_to_crypt_op(&compat_cop, &kcop->cop);

	if (kcop->cop.num_src_bufs > MAX_CRYPTO_BUFFERS ||
	    kcop->cop.num_dst_bufs > MAX_CRYPTO_BUFFERS) {
		dbgp(2, "too many buffers, src: %d, dst: %d\n",
		     kcop->cop.num_src_bufs, kcop->cop.num_dst_bufs);
		return -EINVAL;
	}

	return fill_kcop_from_cop(kcop, fcr);
}

static int compat_kcop_to_user(struct kernel_crypt_op *kcop,
			       struct fcrypt *fcr, void __user *arg)
{
	int ret;
	struct compat_crypt_op compat_cop;

	ret = fill_cop_from_kcop(kcop, fcr);
	if (unlikely(ret)) {
		dbgp(2, "Error in fill_cop_from_kcop");
		return ret;
	}
	crypt_op_to_compat(&kcop->cop, &compat_cop);

	if (unlikely(copy_to_user(arg, &compat_cop, sizeof(compat_cop)))) {
		dbgp(2, "Error copying to user");
		return -EFAULT;
	}
	return 0;
}

static long aml_crypto_dev_compat_ioctl(struct file *filp,
					unsigned int cmd, unsigned long arg_)
{
	void __user *arg = (void __user *)arg_;
	struct crypt_priv *pcr = filp->private_data;
	struct fcrypt *fcr;
	struct kernel_crypt_op kcop;
	int ret = 0;

	if (unlikely(!pcr)) {
		dbgp(2, "empty pcr\n");
		return -EINVAL;
	}

	fcr = &pcr->fcrypt;

	switch (cmd) {
	case CREATE_SESSION:
		fallthrough;
	case CLOSE_SESSION:
		return aml_crypto_dev_ioctl(filp, cmd, arg_);
	case DO_CRYPTO_COMPAT:
		ret = compat_kcop_from_user(&kcop, fcr, arg);
		if (unlikely(ret))
			return ret;

		ret = crypto_run(fcr, &kcop);
		if (unlikely(ret))
			return ret;

		return compat_kcop_to_user(&kcop, fcr, arg);
	default:
		dbgp(3, "unknown cmd: %d\n", cmd);
		ret = -EINVAL;
	}

	return ret;
}
#endif

static const struct file_operations aml_crypto_dev_fops = {
	.owner = THIS_MODULE,
	.open = aml_crypto_dev_open,
	.release = aml_crypto_dev_release,
	.unlocked_ioctl = aml_crypto_dev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = aml_crypto_dev_compat_ioctl,
#endif /* CONFIG_COMPAT */
};

static struct miscdevice aml_crypto_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "aml_crypto",
	.fops = &aml_crypto_dev_fops,
	.mode = 0666,
};

static int aml_crypto_dev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *match;
	struct resource *res_irq = 0;
	const struct meson_crypto_dev_data *priv_data = NULL;
	int err = 0;
	u8 thread = 0;

	crypto_dd = devm_kzalloc(dev, sizeof(struct aml_crypto_dev),
				 GFP_KERNEL);
	if (!crypto_dd) {
		err = -ENOMEM;
		goto error;
	}

	match = of_match_device(aml_crypto_dev_dt_match, &pdev->dev);
	if (!match) {
		dev_err(dev, "%s: cannot find match dt\n", __func__);
		err = -EINVAL;
		goto error;
	}

	res_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);

	of_property_read_u8(pdev->dev.of_node, "thread", &thread);

	priv_data = match->data;
	crypto_dd->dev = dev;
	crypto_dd->thread = thread;
	crypto_dd->status = priv_data->status + thread;
	crypto_dd->algo_cap = priv_data->algo_cap;
	crypto_dd->irq = res_irq->start;
	crypto_dd->processing = NULL;
	init_waitqueue_head(&crypto_dd->waiter);
	mutex_init(&crypto_dd->lock);
	platform_set_drvdata(pdev, crypto_dd);

#if !USE_BUSY_POLLING
	err = devm_request_irq(dev, crypto_dd->irq, aml_crypto_dev_irq,
			       IRQF_SHARED, "aml-aes", crypto_dd);
	if (err) {
		dev_err(dev, "unable to request aes irq.\n");
		err = -EINVAL;
		goto error;
	}
#endif
	err = misc_register(&aml_crypto_device);
	if (unlikely(err)) {
		dev_err(dev, "registration of /dev/aml_crypto failed\n");
		err = -EPERM;
		goto error;
	}

	dev_info(dev, "Aml crypto device\n");

	return err;

error:
	dev_err(dev, "initialization failed.\n");

	return err;
}

static int aml_crypto_dev_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	static struct aml_crypto_dev *crypto_dd;
	const struct of_device_id *match;

	crypto_dd = platform_get_drvdata(pdev);
	if (!crypto_dd)
		return -ENODEV;
	match = of_match_device(aml_crypto_dev_dt_match, &pdev->dev);
	if (!match) {
		dev_err(dev, "%s: cannot find match dt\n", __func__);
		return -EINVAL;
	}

#if !USE_BUSY_POLLING
	devm_free_irq(dev, crypto_dd->irq, crypto_dd);
#endif

	misc_deregister(&aml_crypto_device);
	return 0;
}

static struct platform_driver aml_crypto_dev_driver = {
	.probe		= aml_crypto_dev_probe,
	.remove		= aml_crypto_dev_remove,
	.driver		= {
		.name	= "aml_crypto_dev",
		.owner	= THIS_MODULE,
		.of_match_table = aml_crypto_dev_dt_match,
	},
};

int __init aml_crypto_device_driver_init(void)
{
	return platform_driver_register(&aml_crypto_dev_driver);
}

void aml_crypto_device_driver_exit(void)
{
	platform_driver_unregister(&aml_crypto_dev_driver);
}
