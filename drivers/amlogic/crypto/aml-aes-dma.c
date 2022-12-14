// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/hw_random.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/of_device.h>

#include <linux/device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/crypto.h>
#include <linux/cryptohash.h>
#include <crypto/scatterwalk.h>
#include <crypto/algapi.h>
#include <crypto/aes.h>
#include <crypto/hash.h>
#include <crypto/internal/hash.h>
#include <linux/of_platform.h>
#include <crypto/skcipher.h>
#include "aml-crypto-dma.h"

/* AES flags */
#define AES_FLAGS_MODE_MASK	0x07
#define AES_FLAGS_ENCRYPT   BIT(0)
#define AES_FLAGS_CBC       BIT(1)
#define AES_FLAGS_CTR       BIT(2)

#define AES_FLAGS_INIT      BIT(8)
#define AES_FLAGS_DMA       BIT(9)
#define AES_FLAGS_FAST      BIT(10)
#define AES_FLAGS_BUSY      BIT(11)
#define AES_FLAGS_ERROR     BIT(12)

#define AML_AES_QUEUE_LENGTH	50
#define AML_AES_DMA_THRESHOLD		16

#define DEFAULT_AUTOSUSPEND_DELAY	1000

struct aml_aes_dev;

struct aml_aes_ctx {
	struct aml_aes_dev *dd;

	int		keylen;
	u32		key[AES_KEYSIZE_256 / sizeof(u32)];

	u16		block_size;
	struct crypto_skcipher	*fallback;

	int kte;
};

struct aml_aes_reqctx {
	unsigned long mode;
};

struct aml_aes_dev {
	struct list_head	list;

	struct aml_aes_ctx	*ctx;
	struct device		*dev;
	struct device		*parent;
	int	irq;

	unsigned long		flags;
	int	err;

	struct aml_dma_dev      *dma;
	u32 thread;
	u32 status;
	u8 link_mode;
	struct crypto_queue	queue;

	struct tasklet_struct	done_task;
	struct tasklet_struct	queue_task;

	struct ablkcipher_request	*req;
	size_t	total;
	size_t	fast_total;

	struct scatterlist	*in_sg;
	size_t			in_offset;
	struct scatterlist	*out_sg;
	size_t			out_offset;

	size_t	buflen;
	size_t	dma_size;

	void	*buf_in;
	dma_addr_t	dma_addr_in;

	void	*buf_out;
	dma_addr_t	dma_addr_out;

	void	*descriptor;
	dma_addr_t	dma_descript_tab;

	void	*sg_dsc_in;
	dma_addr_t	dma_sg_dsc_in;
	void	*sg_dsc_out;
	dma_addr_t	dma_sg_dsc_out;

	u8  iv_swap;
	u8  set_key_iv_separately;
};

struct aml_aes_drv {
	struct list_head	dev_list;
	spinlock_t		lock; /* spinlock for device list */
};

struct aml_aes_info {
	struct crypto_alg *algs;
	u32 num_algs;
};

static struct aml_aes_drv aml_aes = {
	.dev_list = LIST_HEAD_INIT(aml_aes.dev_list),
	.lock = __SPIN_LOCK_UNLOCKED(aml_aes.lock),
};

static int aml_aes_crypt_dma_stop(struct aml_aes_dev *dd);
static int set_aes_kl_key_iv(struct aml_aes_dev *dd, u32 *key,
			     u32 keylen, u32 *iv, u8 swap)
{
	struct device *dev = dd->dev;
	struct dma_dsc *dsc = dd->descriptor;
	u32 *key_iv = kzalloc(DMA_KEY_IV_BUF_SIZE, GFP_ATOMIC);

	s32 len = keylen;
	dma_addr_t dma_addr_key = 0;

	if (!key_iv) {
		dev_err(dev, "error allocating key_iv buffer\n");
		return -EINVAL;
	}

	if (iv) {
		u32 *piv = key_iv; // + 8;

		if (swap) {
			*(piv + 3) = swap_ulong32(*iv);
			*(piv + 2) = swap_ulong32(*(iv + 1));
			*(piv + 1) = swap_ulong32(*(iv + 2));
			*(piv + 0) = swap_ulong32(*(iv + 3));
		} else {
			memcpy(piv, iv, 16);
		}
	}

	len = DMA_KEY_IV_BUF_SIZE; /* full key storage */

	dma_addr_key = dma_map_single(dd->parent, key_iv,
				      DMA_KEY_IV_BUF_SIZE, DMA_TO_DEVICE);

	if (dma_mapping_error(dd->parent, dma_addr_key)) {
		dev_err(dev, "error mapping dma_addr_key\n");
		kfree(key_iv);
		return -EINVAL;
	}

	dsc[0].src_addr = (u32)(0xffffff00 | dd->ctx->kte);
	dsc[0].tgt_addr = 0;
	dsc[0].dsc_cfg.d32 = 0;
	dsc[0].dsc_cfg.b.length = keylen;
	dsc[0].dsc_cfg.b.mode = MODE_KEY;
	dsc[0].dsc_cfg.b.eoc = 0;
	dsc[0].dsc_cfg.b.owner = 1;

	if (iv) {
		dsc[1].src_addr = (u32)dma_addr_key;
		dsc[1].tgt_addr = 32;
		dsc[1].dsc_cfg.d32 = 0;
		dsc[1].dsc_cfg.b.length = 16;
		dsc[1].dsc_cfg.b.mode = MODE_KEY;
		dsc[1].dsc_cfg.b.eoc = 1;
		dsc[1].dsc_cfg.b.owner = 1;
	} else {
		dsc[0].dsc_cfg.b.eoc = 1;
	}

	aml_write_crypto_reg(dd->thread,
			     (uintptr_t)dd->dma_descript_tab | 2);
	aml_dma_debug(dsc, 1, __func__, dd->thread, dd->status);
	while (aml_read_crypto_reg(dd->status) == 0)
		;
	aml_write_crypto_reg(dd->status, 0xf);
	dma_unmap_single(dd->parent, dma_addr_key,
			 DMA_KEY_IV_BUF_SIZE, DMA_TO_DEVICE);

	kfree(key_iv);
	return 0;
}

static int set_aes_key_iv(struct aml_aes_dev *dd, u32 *key,
			  u32 keylen, u32 *iv, u8 swap)
{
	struct device *dev = dd->dev;
	struct dma_dsc *dsc = dd->descriptor;
	u32 *key_iv = kzalloc(DMA_KEY_IV_BUF_SIZE, GFP_ATOMIC);

	dma_addr_t dma_addr_key = 0;
	u8 status = 0;
	int err = 0;
	/* Basically, 1 dsc is enough for setting both key and iv
	 * into full internal storage(48 bytes).
	 * If platform (ex: AXG) cannot set full internal storage with
	 * 1 dsc, then we need 2 ~ 3(with iv) dscs to achieve this.
	 */
	u32 num_of_dsc = 1;

	if (!key_iv) {
		dev_err(dev, "error allocating key_iv buffer\n");
		return -EINVAL;
	}

	if (key)
		memcpy(key_iv, key, keylen);

	if (iv) {
		u32 *piv = key_iv + 8;

		if (swap) {
			*(piv + 3) = swap_ulong32(*iv);
			*(piv + 2) = swap_ulong32(*(iv + 1));
			*(piv + 1) = swap_ulong32(*(iv + 2));
			*(piv + 0) = swap_ulong32(*(iv + 3));
		} else {
			memcpy(piv, iv, 16);
		}
	}

	dma_addr_key = dma_map_single(dd->parent, key_iv,
				      DMA_KEY_IV_BUF_SIZE, DMA_TO_DEVICE);

	if (dma_mapping_error(dd->parent, dma_addr_key)) {
		dev_err(dev, "error mapping dma_addr_key\n");
		kfree(key_iv);
		return -EINVAL;
	}

	if (!dd->set_key_iv_separately) {
		dsc[0].src_addr = (u32)dma_addr_key;
		dsc[0].tgt_addr = 0;
		dsc[0].dsc_cfg.d32 = 0;
		dsc[0].dsc_cfg.b.mode = MODE_KEY;
		dsc[0].dsc_cfg.b.owner = 1;
		dsc[0].dsc_cfg.b.length = DMA_KEY_IV_BUF_SIZE;
		dsc[0].dsc_cfg.b.eoc = 1;
	} else {
		dsc[0].src_addr = (u32)dma_addr_key;
		dsc[0].tgt_addr = 0;
		dsc[0].dsc_cfg.d32 = 0;
		dsc[0].dsc_cfg.b.mode = MODE_KEY;
		dsc[0].dsc_cfg.b.owner = 1;
		dsc[0].dsc_cfg.b.length = 16;

		dsc[1].src_addr = (u32)(dma_addr_key + 16);
		dsc[1].tgt_addr = 16;
		dsc[1].dsc_cfg.d32 = 0;
		dsc[1].dsc_cfg.b.mode = MODE_KEY;
		dsc[1].dsc_cfg.b.owner = 1;
		dsc[1].dsc_cfg.b.length = 16;

		if (iv) {
			dsc[2].src_addr = (u32)(dma_addr_key + 32);
			dsc[2].tgt_addr = 32;
			dsc[2].dsc_cfg.d32 = 0;
			dsc[2].dsc_cfg.b.length = 16;
			dsc[2].dsc_cfg.b.mode = MODE_KEY;
			dsc[2].dsc_cfg.b.owner = 1;
			dsc[2].dsc_cfg.b.eoc = 1;
			num_of_dsc = 3;
		} else {
			/* If there is no iv, make last dsc as EOC */
			dsc[1].dsc_cfg.b.eoc = 1;
			num_of_dsc = 2;
		}
	}

	aml_dma_debug(dsc, num_of_dsc, __func__, dd->thread, dd->status);
#if DMA_IRQ_MODE
	aml_write_crypto_reg(dd->thread,
			     (uintptr_t)dd->dma_descript_tab | 2);
	while (aml_read_crypto_reg(dd->status) == 0)
		;
	status = aml_read_crypto_reg(dd->status);
	if (status & DMA_STATUS_KEY_ERROR) {
		dev_err(dev, "hw crypto failed.\n");
		err = -EINVAL;
	}
	aml_write_crypto_reg(dd->status, 0xf);
#else
	status = aml_dma_do_hw_crypto(dd->dma, dsc, num_of_dsc, dd->dma_descript_tab,
			     1, DMA_FLAG_AES_IN_USE);
	aml_dma_debug(dsc, num_of_dsc, "end aes keyiv", dd->thread, dd->status);
	if (status & DMA_STATUS_KEY_ERROR) {
		dev_err(dev, "hw crypto failed.\n");
		err = -EINVAL;
	}
#endif
	dma_unmap_single(dd->parent, dma_addr_key,
			 DMA_KEY_IV_BUF_SIZE, DMA_TO_DEVICE);

	kfree(key_iv);
	return err;
}

static size_t aml_aes_sg_copy(struct scatterlist **sg, size_t *offset,
			      void *buf, size_t buflen, size_t total, int out)
{
	size_t count, off = 0;

	while (buflen && total) {
		count = min((*sg)->length - *offset, total);
		count = min(count, buflen);

		if (!count)
			return off;

		scatterwalk_map_and_copy(buf + off, *sg, *offset, count, out);

		off += count;
		buflen -= count;
		*offset += count;
		total -= count;

		if (*offset == (*sg)->length) {
			*sg = sg_next(*sg);
			if (*sg)
				*offset = 0;
			else
				total = 0;
		}
	}

	return off;
}

static size_t aml_aes_sg_dma(struct aml_aes_dev *dd, struct dma_dsc *dsc,
			     size_t total)
{
	struct device *dev = dd->dev;
	u32 i = 0;
	int err = 0;
	struct scatterlist *in_sg = dd->in_sg;
	struct scatterlist *out_sg = dd->out_sg;
	u32 in_nents = 0, out_nents = 0;
	struct dma_sg_dsc *sg_dsc = NULL;

	in_nents = sg_nents(in_sg);
	out_nents = sg_nents(out_sg);

	if (dd->in_sg != dd->out_sg) {
		err = dma_map_sg(dd->parent, dd->in_sg, in_nents, DMA_TO_DEVICE);
		if (!err) {
			dev_err(dev, "dma_map_sg() error\n");
			return 0;
		}

		err = dma_map_sg(dd->parent, dd->out_sg, out_nents,
				 DMA_FROM_DEVICE);
		if (!err) {
			dev_err(dev, "dma_map_sg() error\n");
			dma_unmap_sg(dd->parent, dd->in_sg, in_nents,
				     DMA_TO_DEVICE);
			return 0;
		}
	} else {
		err = dma_map_sg(dd->parent, dd->in_sg, in_nents,
				 DMA_BIDIRECTIONAL);
		if (!err) {
			dev_err(dev, "dma_map_sg() error\n");
			return 0;
		}
		dma_sync_sg_for_device(dd->parent, dd->in_sg,
				       in_nents, DMA_TO_DEVICE);
	}

#if DMA_IRQ_MODE
	dd->sg_dsc_in = dma_alloc_coherent(dd->parent,
			in_nents * sizeof(struct dma_sg_dsc),
			&dd->dma_sg_dsc_in, GFP_ATOMIC | GFP_DMA);
	if (!dd->sg_dsc_in) {
		dev_err(dev, "dma_alloc_coherent() for input error\n");
		return 0;
	}

	dd->sg_dsc_out = dma_alloc_coherent(dd->parent,
			out_nents * sizeof(struct dma_sg_dsc),
			&dd->dma_sg_dsc_out, GFP_ATOMIC | GFP_DMA);
	if (!dd->dma_sg_dsc_out) {
		dev_err(dev, "dma_alloc_coherent() for output error\n");
		dma_free_coherent(dd->parent, in_nents * sizeof(struct dma_sg_dsc),
				dd->sg_dsc_in, dd->dma_sg_dsc_in);
		return 0;
	}
#else
	dd->sg_dsc_in = dma_alloc_coherent(dd->parent,
			in_nents * sizeof(struct dma_sg_dsc),
			&dd->dma_sg_dsc_in, GFP_KERNEL | GFP_DMA);
	if (!dd->sg_dsc_in) {
		dev_err(dev, "dma_alloc_coherent() for input error\n");
		return 0;
	}

	dd->sg_dsc_out = dma_alloc_coherent(dd->parent,
			out_nents * sizeof(struct dma_sg_dsc),
			&dd->dma_sg_dsc_out, GFP_KERNEL | GFP_DMA);
	if (!dd->dma_sg_dsc_out) {
		dev_err(dev, "dma_alloc_coherent() for output error\n");
		dma_free_coherent(dd->parent, in_nents * sizeof(struct dma_sg_dsc),
				dd->sg_dsc_in, dd->dma_sg_dsc_in);
		return 0;
	}
#endif

	sg_dsc = dd->sg_dsc_in;
	in_sg = dd->in_sg;
	for (i = 0; i < in_nents; i++) {
		sg_dsc[i].dsc_cfg.d32 = 0;
		sg_dsc[i].dsc_cfg.b.valid = 1;
		sg_dsc[i].dsc_cfg.b.eoc = i == (in_nents - 1) ? 1 : 0;
		sg_dsc[i].dsc_cfg.b.length = in_sg->length;
		sg_dsc[i].addr = in_sg->dma_address;
		in_sg = sg_next(in_sg);
	}
	WARN_ON(in_sg);

	out_sg = dd->out_sg;
	sg_dsc = dd->sg_dsc_out;
	for (i = 0; i < out_nents; i++) {
		sg_dsc[i].dsc_cfg.d32 = 0;
		sg_dsc[i].dsc_cfg.b.valid = 1;
		sg_dsc[i].dsc_cfg.b.eoc = i == (out_nents - 1) ? 1 : 0;
		sg_dsc[i].dsc_cfg.b.length = out_sg->length;
		sg_dsc[i].addr = out_sg->dma_address;
		out_sg = sg_next(out_sg);
	}
	WARN_ON(out_sg);

	aml_dma_link_debug(dd->sg_dsc_in, dd->dma_sg_dsc_in, in_nents, __func__);
	aml_dma_link_debug(dd->sg_dsc_out, dd->dma_sg_dsc_out, in_nents, __func__);

	dsc->src_addr = dd->dma_sg_dsc_in;
	dsc->tgt_addr = dd->dma_sg_dsc_out;
	dsc->dsc_cfg.d32 = 0;
	dsc->dsc_cfg.b.length = total;
	dsc->dsc_cfg.b.link_error = 1;

	return total;
}

static struct aml_aes_dev *aml_aes_find_dev(struct aml_aes_ctx *ctx)
{
	struct aml_aes_dev *aes_dd = NULL;
	struct aml_aes_dev *tmp;

	spin_lock_bh(&aml_aes.lock);
	if (!ctx->dd) {
		list_for_each_entry(tmp, &aml_aes.dev_list, list) {
			aes_dd = tmp;
			break;
		}
		ctx->dd = aes_dd;
	} else {
		aes_dd = ctx->dd;
	}

	spin_unlock_bh(&aml_aes.lock);

	return aes_dd;
}

static int aml_aes_hw_init(struct aml_aes_dev *dd)
{
	if (!(dd->flags & AES_FLAGS_INIT)) {
		dd->flags |= AES_FLAGS_INIT;
		dd->err = 0;
	}

	return 0;
}

static void aml_aes_finish_req(struct aml_aes_dev *dd, s32 err)
{
#if DMA_IRQ_MODE
	struct ablkcipher_request *req = dd->req;
#endif
	unsigned long flags;

	dd->flags &= ~AES_FLAGS_BUSY;
	spin_lock_irqsave(&dd->dma->dma_lock, flags);
	dd->dma->dma_busy &= ~DMA_FLAG_MAY_OCCUPY;
	spin_unlock_irqrestore(&dd->dma->dma_lock, flags);
#if DMA_IRQ_MODE
	req->base.complete(&req->base, err);
#endif
}

static int aml_aes_crypt_dma(struct aml_aes_dev *dd, struct dma_dsc *dsc,
			     u32 nents)
{
	u32 op_mode = OP_MODE_ECB;
	u32 i = 0;
#if DMA_IRQ_MODE
	unsigned long flags;
#else
	int err = 0;
	u8 status = 0;
#endif

	if (dd->flags & AES_FLAGS_CBC)
		op_mode = OP_MODE_CBC;
	else if (dd->flags & AES_FLAGS_CTR)
		op_mode = OP_MODE_CTR;

	for (i = 0; i < nents; i++) {
		dsc[i].dsc_cfg.b.enc_sha_only = dd->flags & AES_FLAGS_ENCRYPT;
		dsc[i].dsc_cfg.b.mode =
			((dd->ctx->keylen == AES_KEYSIZE_128) ? MODE_AES128 :
			 ((dd->ctx->keylen == AES_KEYSIZE_192) ?
			  MODE_AES192 : MODE_AES256));
		dsc[i].dsc_cfg.b.op_mode = op_mode;
		dsc[i].dsc_cfg.b.eoc = (i == (nents - 1));
		dsc[i].dsc_cfg.b.owner = 1;
	}

	aml_dma_debug(dsc, nents, __func__, dd->thread, dd->status);

	/* Start DMA transfer */
#if DMA_IRQ_MODE
	spin_lock_irqsave(&dd->dma->dma_lock, flags);
	dd->dma->dma_busy |= DMA_FLAG_AES_IN_USE;
	spin_unlock_irqrestore(&dd->dma->dma_lock, flags);

	dd->flags |= AES_FLAGS_DMA;
	aml_write_crypto_reg(dd->thread, dd->dma_descript_tab | 2);
	return -EINPROGRESS;
#else
	dd->flags |= AES_FLAGS_DMA;
	status = aml_dma_do_hw_crypto(dd->dma, dsc, nents, dd->dma_descript_tab,
			     1, DMA_FLAG_AES_IN_USE);
	if (status & DMA_STATUS_KEY_ERROR)
		dd->flags |= AES_FLAGS_ERROR;
	aml_dma_debug(dsc, nents, "end aes", dd->thread, dd->status);
	err = aml_aes_crypt_dma_stop(dd);
	if (!err) {
		err = dd->flags & AES_FLAGS_ERROR;
		dd->flags = (dd->flags & ~AES_FLAGS_ERROR);
	}
	return err;
#endif

}

static int aml_aes_crypt_dma_start(struct aml_aes_dev *dd)
{
	int err = 0;
	size_t count = 0;
	dma_addr_t addr_in, addr_out;
	struct dma_dsc *dsc = dd->descriptor;
	u32 nents;

	/* slow dma */
	/* use cache buffers */
	count = aml_aes_sg_copy(&dd->in_sg, &dd->in_offset,
				dd->buf_in, dd->buflen, dd->total, 0);
	addr_in = dd->dma_addr_in;
	addr_out = dd->dma_addr_out;
	dd->dma_size = count;
	dma_sync_single_for_device(dd->parent, addr_in,
				   ((dd->dma_size + AES_BLOCK_SIZE - 1)
				   / AES_BLOCK_SIZE) * AES_BLOCK_SIZE,
				   DMA_TO_DEVICE);
	dsc->src_addr = (u32)addr_in;
	dsc->tgt_addr = (u32)addr_out;
	dsc->dsc_cfg.d32 = 0;
	/* We align data to AES_BLOCK_SIZE for old aml-dma devices */
	dsc->dsc_cfg.b.length = ((count + AES_BLOCK_SIZE - 1)
				 / AES_BLOCK_SIZE) * AES_BLOCK_SIZE;
	nents = 1;
	dd->flags &= ~AES_FLAGS_FAST;
	dbgp(1, "use slow dma: cnt:%zd, len:%d\n",
	     count, dsc->dsc_cfg.b.length);

	dd->total -= count;

	err = aml_aes_crypt_dma(dd, dsc, nents);

	return err;
}

static int aml_aes_crypt_dma_link_mode_start(struct aml_aes_dev *dd)
{
	int err = 0;
	size_t count = 0;
	struct dma_dsc *dsc = dd->descriptor;

	count = aml_aes_sg_dma(dd, dsc, dd->total);
	dd->flags |= AES_FLAGS_FAST;
	dd->fast_total = count;
	dd->total -= dd->fast_total;

	/* install IV */
	if ((dd->flags & AES_FLAGS_CBC) && !(dd->flags & AES_FLAGS_ENCRYPT))
		scatterwalk_map_and_copy(dd->req->info,
					 dd->in_sg,
					 dd->fast_total - 16,
					 16, 0);

	/* using 1 entry in link mode */
	err = aml_aes_crypt_dma(dd, dsc, 1);

	return err;
}

static int aml_aes_write_ctrl(struct aml_aes_dev *dd)
{
	int err = 0;
	u32 *iv = NULL;
	u8 iv_swap = 0;

	err = aml_aes_hw_init(dd);

	if (err)
		return err;

	if (dd->flags & AES_FLAGS_CBC)
		iv = dd->req->info;
	else if (dd->flags & AES_FLAGS_CTR) {
		iv = dd->req->info;
		iv_swap = dd->iv_swap;
	}

	if (dd->ctx->kte >= 0)
		err = set_aes_kl_key_iv(dd, dd->ctx->key, dd->ctx->keylen,
					iv, iv_swap);
	else
		err = set_aes_key_iv(dd, dd->ctx->key, dd->ctx->keylen,
				     iv, iv_swap);


	return err;
}

static int aml_aes_handle_queue(struct aml_aes_dev *dd,
				struct ablkcipher_request *req)
{
	struct device *dev = dd->dev;
#if DMA_IRQ_MODE
	struct crypto_async_request *async_req, *backlog;
#endif
	struct aml_aes_ctx *ctx;
	struct aml_aes_reqctx *rctx;
	s32 err = 0;
#if DMA_IRQ_MODE
	s32 ret = 0;
#endif

#if DMA_IRQ_MODE
	unsigned long flags;
	spin_lock_irqsave(&dd->dma->dma_lock, flags);
	if (req)
		ret = ablkcipher_enqueue_request(&dd->queue, req);

	if ((dd->flags & AES_FLAGS_BUSY) || dd->dma->dma_busy) {
		spin_unlock_irqrestore(&dd->dma->dma_lock, flags);
		return ret;
	}
	backlog = crypto_get_backlog(&dd->queue);
	async_req = crypto_dequeue_request(&dd->queue);
	if (async_req) {
		dd->flags |= AES_FLAGS_BUSY;
		dd->dma->dma_busy |= DMA_FLAG_MAY_OCCUPY;
	}
	spin_unlock_irqrestore(&dd->dma->dma_lock, flags);

	if (!async_req) {
		pm_runtime_mark_last_busy(dd->parent);
		pm_runtime_put_autosuspend(dd->parent);
		return ret;
	}

	if (backlog)
		backlog->complete(backlog, -EINPROGRESS);

	req = ablkcipher_request_cast(async_req);
#endif
	/* assign new request to device */
	dd->req = req;
	dd->total = req->nbytes;
	dd->in_offset = 0;
	dd->in_sg = req->src;
	dd->out_offset = 0;
	dd->out_sg = req->dst;

	rctx = ablkcipher_request_ctx(req);
	ctx = crypto_ablkcipher_ctx(crypto_ablkcipher_reqtfm(req));
	rctx->mode &= AES_FLAGS_MODE_MASK;
	dd->flags = (dd->flags & ~AES_FLAGS_MODE_MASK) | rctx->mode;
	dd->flags = (dd->flags & ~AES_FLAGS_ERROR);
	dd->ctx = ctx;
	ctx->dd = dd;

	err = aml_aes_write_ctrl(dd);
	if (!err) {
		if (dd->total % AML_AES_DMA_THRESHOLD == 0 ||
		    (dd->flags & AES_FLAGS_CTR)) {
			if (dd->link_mode && !disable_link_mode) {
				err = aml_aes_crypt_dma_link_mode_start(dd);
			} else {
				do {
					err = aml_aes_crypt_dma_start(dd);
				} while (!err && dd->total);
			}
		} else {
			dev_err(dev, "size %zd is not multiple of %d",
				dd->total, AML_AES_DMA_THRESHOLD);
			err = -EINVAL;
		}
	}
	if (err != -EINPROGRESS) {
		/* aes_task will not finish it, so do it here */
		aml_aes_finish_req(dd, err);
#if DMA_IRQ_MODE
		tasklet_schedule(&dd->queue_task);
#endif
	}

#if DMA_IRQ_MODE
	return ret;
#else
	if (err)
		return -EAGAIN;
	else
		return 0;
#endif
}

int aml_aes_process(struct ablkcipher_request *req)
{
	struct aml_aes_ctx *tctx = crypto_tfm_ctx(req->base.tfm);
	struct aml_aes_dev *dd = tctx->dd;

	return aml_aes_handle_queue(dd, req);
}

/* Increment 128-bit counter */
static void aml_aes_ctr_update_iv(unsigned char *val, unsigned int value)
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

static int aml_aes_crypt_dma_stop(struct aml_aes_dev *dd)
{
	struct device *dev = dd->dev;
	int err = -EINVAL;
	size_t count;
	u32 in_nents = 0, out_nents = 0;

	if (dd->flags & AES_FLAGS_DMA) {
		err = 0;
		if  (dd->flags & AES_FLAGS_FAST) {
			in_nents = sg_nents(dd->in_sg);
			out_nents = sg_nents(dd->out_sg);
			if (dd->in_sg != dd->out_sg) {
				dma_unmap_sg(dd->parent, dd->in_sg, in_nents,
					     DMA_TO_DEVICE);
				dma_unmap_sg(dd->parent, dd->out_sg, out_nents,
						DMA_FROM_DEVICE);
			} else {
				dma_sync_sg_for_cpu(dd->parent, dd->in_sg, in_nents,
						DMA_FROM_DEVICE);
				dma_unmap_sg(dd->parent, dd->in_sg, in_nents,
						DMA_BIDIRECTIONAL);
			}

			aml_dma_link_debug(dd->sg_dsc_in, dd->dma_sg_dsc_in, in_nents, __func__);
			aml_dma_link_debug(dd->sg_dsc_out, dd->dma_sg_dsc_out, in_nents, __func__);

			dma_free_coherent(dd->parent,
					  in_nents * sizeof(struct dma_sg_dsc),
					  dd->sg_dsc_in, dd->dma_sg_dsc_in);
			dma_free_coherent(dd->parent,
					  out_nents * sizeof(struct dma_sg_dsc),
					  dd->sg_dsc_out, dd->dma_sg_dsc_out);

			/* install IV */
			if (dd->flags & AES_FLAGS_CBC) {
				u32 length = dd->fast_total - AES_BLOCK_SIZE;

				if (dd->flags & AES_FLAGS_ENCRYPT) {
					scatterwalk_map_and_copy(dd->req->info,
								 dd->out_sg,
								 length,
								 AES_BLOCK_SIZE,
								 0);
				}
			} else if (dd->flags & AES_FLAGS_CTR) {
				u32 dma_nblock =
					(dd->fast_total + AES_BLOCK_SIZE - 1)
					/ AES_BLOCK_SIZE;

				aml_aes_ctr_update_iv(dd->req->info,
						      dma_nblock);
			}
		} else {
			dma_sync_single_for_cpu(dd->parent,
						dd->dma_addr_out,
						((dd->dma_size +
						  AES_BLOCK_SIZE - 1)
						/ AES_BLOCK_SIZE) *
						AES_BLOCK_SIZE,
						DMA_FROM_DEVICE);

			/* copy data */
			count = aml_aes_sg_copy(&dd->out_sg, &dd->out_offset,
						dd->buf_out, dd->buflen,
						dd->dma_size, 1);
			if (count != dd->dma_size) {
				err = -EINVAL;
				dev_err(dev, "not all data converted: %zu\n",
					count);
			}
			/* install IV */
			if (dd->flags & AES_FLAGS_CBC) {
				if (dd->flags & AES_FLAGS_ENCRYPT) {
					memcpy(dd->req->info, dd->buf_out +
					       dd->dma_size - AES_BLOCK_SIZE,
					       AES_BLOCK_SIZE);
				} else {
					memcpy(dd->req->info, dd->buf_in +
					       dd->dma_size - AES_BLOCK_SIZE,
					       AES_BLOCK_SIZE);
				}
			} else if (dd->flags & AES_FLAGS_CTR) {
				u32 dma_nblock =
					(dd->dma_size + AES_BLOCK_SIZE - 1)
					/ AES_BLOCK_SIZE;

				aml_aes_ctr_update_iv(dd->req->info,
						      dma_nblock);
			}
		}
		dd->flags &= ~AES_FLAGS_DMA;
	}

	return err;
}

static int aml_aes_buff_init(struct aml_aes_dev *dd)
{
	struct device *dev = dd->dev;
	int err = -ENOMEM;

	dd->buf_in = (void *)__get_free_pages(GFP_KERNEL, 0);
	dd->buf_out = (void *)__get_free_pages(GFP_KERNEL, 0);
	dd->buflen = PAGE_SIZE;
	dd->buflen &= ~(AES_BLOCK_SIZE - 1);

	if (!dd->buf_in || !dd->buf_out) {
		dev_err(dev, "unable to alloc pages.\n");
		goto err_alloc;
	}

	dd->descriptor =
		dmam_alloc_coherent(dd->parent,
				   MAX_NUM_TABLES * sizeof(struct dma_dsc),
				   &dd->dma_descript_tab, GFP_KERNEL | GFP_DMA);
	if (!dd->descriptor) {
		dev_err(dev, "dma descriptor error\n");
		err = -EINVAL;
		goto err_map_in;
	}

	/* MAP here */
	dd->dma_addr_in = dma_map_single(dd->parent, dd->buf_in,
					 dd->buflen, DMA_TO_DEVICE);
	if (dma_mapping_error(dd->parent, dd->dma_addr_in)) {
		dev_err(dev, "dma %zd bytes error\n", dd->buflen);
		err = -EINVAL;
		goto err_map_in;
	}

	dd->dma_addr_out = dma_map_single(dd->parent, dd->buf_out,
					  dd->buflen, DMA_FROM_DEVICE);
	if (dma_mapping_error(dd->parent, dd->dma_addr_out)) {
		dev_err(dev, "dma %zd bytes error\n", dd->buflen);
		err = -EINVAL;
		goto err_map_out;
	}

	return 0;

err_map_out:
	dma_unmap_single(dd->parent, dd->dma_addr_in, dd->buflen,
			 DMA_TO_DEVICE);
err_map_in:
	free_page((uintptr_t)dd->buf_out);
	free_page((uintptr_t)dd->buf_in);
err_alloc:
	if (err)
		dev_err(dev, "error: %d\n", err);
	return err;
}

static void aml_aes_buff_cleanup(struct aml_aes_dev *dd)
{
	dma_unmap_single(dd->parent, dd->dma_addr_out, dd->buflen,
			 DMA_FROM_DEVICE);
	dma_unmap_single(dd->parent, dd->dma_addr_in, dd->buflen,
			 DMA_TO_DEVICE);
	dmam_free_coherent(dd->parent, MAX_NUM_TABLES * sizeof(struct dma_dsc),
			dd->descriptor, dd->dma_descript_tab);
	free_page((uintptr_t)dd->buf_out);
	free_page((uintptr_t)dd->buf_in);
}

static int aml_aes_crypt(struct ablkcipher_request *req, unsigned long mode)
{
	struct aml_aes_ctx *ctx =
		crypto_ablkcipher_ctx(crypto_ablkcipher_reqtfm(req));
	struct aml_aes_reqctx *rctx = ablkcipher_request_ctx(req);
	struct aml_aes_dev *dd;
	int ret;

	if (!IS_ALIGNED(req->nbytes, AES_BLOCK_SIZE) &&
	    !(mode & AES_FLAGS_CTR)) {
		pr_err("request size is not exact amount of AES blocks\n");
		return -EINVAL;
	}
	ctx->block_size = AES_BLOCK_SIZE;

	if (ctx->fallback && ctx->keylen == AES_KEYSIZE_192) {
		struct skcipher_request *subreq = NULL;

		subreq = kzalloc(sizeof(*subreq) +
				 crypto_skcipher_reqsize(ctx->fallback),
				 GFP_ATOMIC);
		if (!subreq)
			return -ENOMEM;

		skcipher_request_set_tfm(subreq, ctx->fallback);
		skcipher_request_set_callback(subreq, req->base.flags, NULL,
					      NULL);
		skcipher_request_set_crypt(subreq, req->src, req->dst,
					   req->nbytes, req->info);

		if (mode & AES_FLAGS_ENCRYPT)
			ret = crypto_skcipher_encrypt(subreq);
		else
			ret = crypto_skcipher_decrypt(subreq);

		skcipher_request_free(subreq);
		return ret;
	}

	dd = aml_aes_find_dev(ctx);
	if (!dd)
		return -ENODEV;

	rctx->mode = mode;

	if (pm_runtime_suspended(dd->parent)) {
		ret = pm_runtime_get_sync(dd->parent);
		if (ret < 0) {
			dev_err(dd->parent, "%s: pm_runtime_get_sync fails: %d\n",
				__func__, ret);
			return ret;
		}
	}
#if DMA_IRQ_MODE
	return aml_aes_handle_queue(dd, req);
#else
	return aml_dma_crypto_enqueue_req(dd->dma, &req->base);
#endif
}

static int aml_aes_kl_setkey(struct crypto_ablkcipher *tfm, const u8 *key,
			     unsigned int keylen)
{
	struct aml_aes_ctx *ctx = crypto_ablkcipher_ctx(tfm);
	int ret = 0;

	/* key[0:3] = kte */
	ctx->kte = *(uint32_t *)&key[0];

	if (keylen != AES_KEYSIZE_128 && keylen != AES_KEYSIZE_192 &&
	    keylen != AES_KEYSIZE_256) {
		crypto_ablkcipher_set_flags(tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);
		pr_err("aml-aes-lite:invalid keysize: %d\n", keylen);
		return -EINVAL;
	}

	ctx->keylen = keylen;

	if (keylen == AES_KEYSIZE_192) {
		crypto_skcipher_clear_flags(ctx->fallback, CRYPTO_TFM_REQ_MASK);
		crypto_skcipher_set_flags(ctx->fallback, tfm->base.crt_flags &
				CRYPTO_TFM_REQ_MASK);
		ret = crypto_skcipher_setkey(ctx->fallback, key, keylen);
	}

	return ret;
}

static int aml_aes_lite_setkey(struct crypto_ablkcipher *tfm, const u8 *key,
			       unsigned int keylen)
{
	struct aml_aes_ctx *ctx = crypto_ablkcipher_ctx(tfm);
	int ret = 0;

	if (keylen != AES_KEYSIZE_128 && keylen != AES_KEYSIZE_192 &&
	    keylen != AES_KEYSIZE_256) {
		crypto_ablkcipher_set_flags(tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);
		pr_err("aml-aes-lite:invalid keysize: %d\n", keylen);
		return -EINVAL;
	}
	ctx->kte = -1;

	memcpy(ctx->key, key, keylen);
	ctx->keylen = keylen;

	if (keylen == AES_KEYSIZE_192) {
		crypto_skcipher_clear_flags(ctx->fallback, CRYPTO_TFM_REQ_MASK);
		crypto_skcipher_set_flags(ctx->fallback, tfm->base.crt_flags &
				CRYPTO_TFM_REQ_MASK);
		ret = crypto_skcipher_setkey(ctx->fallback, key, keylen);
	}

	return ret;
}

static int aml_aes_ecb_encrypt(struct ablkcipher_request *req)
{
	return aml_aes_crypt(req,
			AES_FLAGS_ENCRYPT);
}

static int aml_aes_ecb_decrypt(struct ablkcipher_request *req)
{
	return aml_aes_crypt(req,
			0);
}

static int aml_aes_cbc_encrypt(struct ablkcipher_request *req)
{
	return aml_aes_crypt(req,
			AES_FLAGS_ENCRYPT | AES_FLAGS_CBC);
}

static int aml_aes_cbc_decrypt(struct ablkcipher_request *req)
{
	return aml_aes_crypt(req,
			AES_FLAGS_CBC);
}

static int aml_aes_ctr_encrypt(struct ablkcipher_request *req)
{
	return aml_aes_crypt(req,
			AES_FLAGS_ENCRYPT | AES_FLAGS_CTR);
}

static int aml_aes_ctr_decrypt(struct ablkcipher_request *req)
{
	/* XXX: use encrypt to replace for decrypt */
	return aml_aes_crypt(req,
			AES_FLAGS_ENCRYPT | AES_FLAGS_CTR);
}

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static int aml_aes_cra_init(struct crypto_tfm *tfm)
{
	struct aml_aes_ctx *ctx = crypto_tfm_ctx(tfm);

	tfm->crt_ablkcipher.reqsize = sizeof(struct aml_aes_reqctx);
	ctx->fallback = NULL;

	return 0;
}

static void aml_aes_cra_exit(struct crypto_tfm *tfm)
{
}

static int aml_aes_setkey(struct crypto_ablkcipher *tfm, const u8 *key,
			  unsigned int keylen)
{
	struct aml_aes_ctx *ctx = crypto_ablkcipher_ctx(tfm);

	if (keylen != AES_KEYSIZE_128 && keylen != AES_KEYSIZE_192 &&
	    keylen != AES_KEYSIZE_256) {
		crypto_ablkcipher_set_flags(tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);
		pr_err("aml-aes:invalid keysize: %d\n", keylen);
		return -EINVAL;
	}

	memcpy(ctx->key, key, keylen);
	ctx->keylen = keylen;
	ctx->kte = -1;

	return 0;
}

static struct crypto_alg aes_algs[] = {
	{
		.cra_name         = "ecb(aes-aml)",
		.cra_driver_name  = "ecb-aes-aml",
		.cra_priority   = 100,
		.cra_flags      = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize  = AES_BLOCK_SIZE,
		.cra_ctxsize    = sizeof(struct aml_aes_ctx),
		.cra_alignmask  = 0xf,
		.cra_type       = &crypto_ablkcipher_type,
		.cra_module     = THIS_MODULE,
		.cra_init       = aml_aes_cra_init,
		.cra_exit       = aml_aes_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    AES_MIN_KEY_SIZE,
			.max_keysize	=    AES_MAX_KEY_SIZE,
			.ivsize		=    AES_BLOCK_SIZE,
			.setkey		=    aml_aes_setkey,
			.encrypt	=    aml_aes_ecb_encrypt,
			.decrypt	=    aml_aes_ecb_decrypt,
		}
	},
	{
		.cra_name         = "cbc(aes-aml)",
		.cra_driver_name  = "cbc-aes-aml",
		.cra_priority   = 100,
		.cra_flags      = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize  = AES_BLOCK_SIZE,
		.cra_ctxsize    = sizeof(struct aml_aes_ctx),
		.cra_alignmask  = 0xf,
		.cra_type       = &crypto_ablkcipher_type,
		.cra_module     = THIS_MODULE,
		.cra_init       = aml_aes_cra_init,
		.cra_exit       = aml_aes_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    AES_MIN_KEY_SIZE,
			.max_keysize	=    AES_MAX_KEY_SIZE,
			.ivsize		=    AES_BLOCK_SIZE,
			.setkey		=    aml_aes_setkey,
			.encrypt	=    aml_aes_cbc_encrypt,
			.decrypt	=    aml_aes_cbc_decrypt,
		}
	},
	{
		.cra_name        = "ctr(aes-aml)",
		.cra_driver_name = "ctr-aes-aml",
		.cra_priority    = 100,
		.cra_flags      = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize  = AES_BLOCK_SIZE,
		.cra_ctxsize    = sizeof(struct aml_aes_ctx),
		.cra_alignmask  = 0xf,
		.cra_type       = &crypto_ablkcipher_type,
		.cra_module     = THIS_MODULE,
		.cra_init       = aml_aes_cra_init,
		.cra_exit       = aml_aes_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    AES_MIN_KEY_SIZE,
			.max_keysize	=    AES_MAX_KEY_SIZE,
			.ivsize		=    AES_BLOCK_SIZE,
			.setkey		=    aml_aes_setkey,
			.encrypt	=    aml_aes_ctr_encrypt,
			.decrypt	=    aml_aes_ctr_decrypt,
		}
	}
};
#endif

static int aml_aes_lite_cra_init(struct crypto_tfm *tfm)
{
	struct aml_aes_ctx *ctx = crypto_tfm_ctx(tfm);
	const char *alg_name = crypto_tfm_alg_name(tfm);
	const char *driver_name = crypto_tfm_alg_driver_name(tfm);
	const u32 flags = CRYPTO_ALG_ASYNC | CRYPTO_ALG_NEED_FALLBACK;
	char alg_to_use[16] = {0};

	tfm->crt_ablkcipher.reqsize = sizeof(struct aml_aes_reqctx);

	/* Driver name with '-kl' is only for installing key by kte.
	 * No need to allocate fallback for it
	 */
	if (!strstr(driver_name, "-kl")) {
		/* Allocate a fallback and abort if it failed. */
		strncpy(alg_to_use, "xxx(aes)", sizeof(alg_to_use));
		memcpy(alg_to_use, alg_name, 3);
		ctx->fallback = crypto_alloc_skcipher(alg_to_use, 0,
						      flags);
		if (IS_ERR(ctx->fallback)) {
			pr_err("aml-aes: fallback '%s' could not be loaded.\n",
			       alg_name);
			return PTR_ERR(ctx->fallback);
		}
	}
	return 0;
}

static void aml_aes_lite_cra_exit(struct crypto_tfm *tfm)
{
	struct aml_aes_ctx *ctx = crypto_tfm_ctx(tfm);

	if (ctx->fallback)
		crypto_free_skcipher(ctx->fallback);

	ctx->fallback = NULL;
}

static struct crypto_alg aes_lite_algs[] = {
	{
		.cra_name         = "ecb(aes-aml)",
		.cra_driver_name  = "ecb-aes-lite-aml",
		.cra_priority   = 100,
		.cra_flags      = CRYPTO_ALG_TYPE_ABLKCIPHER |
			CRYPTO_ALG_ASYNC | CRYPTO_ALG_NEED_FALLBACK,
		.cra_blocksize  = AES_BLOCK_SIZE,
		.cra_ctxsize    = sizeof(struct aml_aes_ctx),
		.cra_alignmask  = 0xf,
		.cra_type       = &crypto_ablkcipher_type,
		.cra_module     = THIS_MODULE,
		.cra_init       = aml_aes_lite_cra_init,
		.cra_exit       = aml_aes_lite_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    AES_MIN_KEY_SIZE,
			.max_keysize	=    AES_MAX_KEY_SIZE,
			.setkey		=    aml_aes_lite_setkey,
			.encrypt	=    aml_aes_ecb_encrypt,
			.decrypt	=    aml_aes_ecb_decrypt,
		}
	},
	{
		.cra_name         = "cbc(aes-aml)",
		.cra_driver_name  = "cbc-aes-lite-aml",
		.cra_priority   = 100,
		.cra_flags      = CRYPTO_ALG_TYPE_ABLKCIPHER |
			CRYPTO_ALG_ASYNC | CRYPTO_ALG_NEED_FALLBACK,
		.cra_blocksize  = AES_BLOCK_SIZE,
		.cra_ctxsize    = sizeof(struct aml_aes_ctx),
		.cra_alignmask  = 0xf,
		.cra_type       = &crypto_ablkcipher_type,
		.cra_module     = THIS_MODULE,
		.cra_init       = aml_aes_lite_cra_init,
		.cra_exit       = aml_aes_lite_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    AES_MIN_KEY_SIZE,
			.max_keysize	=    AES_MAX_KEY_SIZE,
			.ivsize		=    AES_BLOCK_SIZE,
			.setkey		=    aml_aes_lite_setkey,
			.encrypt	=    aml_aes_cbc_encrypt,
			.decrypt	=    aml_aes_cbc_decrypt,
		}
	},
	{
		.cra_name        = "ctr(aes-aml)",
		.cra_driver_name = "ctr-aes-lite-aml",
		.cra_priority    = 100,
		.cra_flags      = CRYPTO_ALG_TYPE_ABLKCIPHER |
			CRYPTO_ALG_ASYNC | CRYPTO_ALG_NEED_FALLBACK,
		.cra_blocksize  = AES_BLOCK_SIZE,
		.cra_ctxsize    = sizeof(struct aml_aes_ctx),
		.cra_alignmask  = 0xf,
		.cra_type       = &crypto_ablkcipher_type,
		.cra_module     = THIS_MODULE,
		.cra_init       = aml_aes_lite_cra_init,
		.cra_exit       = aml_aes_lite_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    AES_MIN_KEY_SIZE,
			.max_keysize	=    AES_MAX_KEY_SIZE,
			.ivsize		=    AES_BLOCK_SIZE,
			.setkey		=    aml_aes_lite_setkey,
			.encrypt	=    aml_aes_ctr_encrypt,
			.decrypt	=    aml_aes_ctr_decrypt,
		}
	},
	{
		.cra_name         = "ecb(aes-kl-aml)",
		.cra_driver_name  = "ecb-aes-kl-aml",
		.cra_priority   = 100,
		.cra_flags      = CRYPTO_ALG_TYPE_ABLKCIPHER |
			CRYPTO_ALG_ASYNC | CRYPTO_ALG_NEED_FALLBACK,
		.cra_blocksize  = AES_BLOCK_SIZE,
		.cra_ctxsize    = sizeof(struct aml_aes_ctx),
		.cra_alignmask  = 0xf,
		.cra_type       = &crypto_ablkcipher_type,
		.cra_module     = THIS_MODULE,
		.cra_init       = aml_aes_lite_cra_init,
		.cra_exit       = aml_aes_lite_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    AES_MIN_KEY_SIZE,
			.max_keysize	=    AES_MAX_KEY_SIZE,
			.setkey		=    aml_aes_kl_setkey,
			.encrypt	=    aml_aes_ecb_encrypt,
			.decrypt	=    aml_aes_ecb_decrypt,
		}
	},
	{
		.cra_name         = "cbc(aes-kl-aml)",
		.cra_driver_name  = "cbc-aes-kl-aml",
		.cra_priority   = 100,
		.cra_flags      = CRYPTO_ALG_TYPE_ABLKCIPHER |
			CRYPTO_ALG_ASYNC | CRYPTO_ALG_NEED_FALLBACK,
		.cra_blocksize  = AES_BLOCK_SIZE,
		.cra_ctxsize    = sizeof(struct aml_aes_ctx),
		.cra_alignmask  = 0xf,
		.cra_type       = &crypto_ablkcipher_type,
		.cra_module     = THIS_MODULE,
		.cra_init       = aml_aes_lite_cra_init,
		.cra_exit       = aml_aes_lite_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    AES_MIN_KEY_SIZE,
			.max_keysize	=    AES_MAX_KEY_SIZE,
			.ivsize		=    AES_BLOCK_SIZE,
			.setkey		=    aml_aes_kl_setkey,
			.encrypt	=    aml_aes_cbc_encrypt,
			.decrypt	=    aml_aes_cbc_decrypt,
		}
	},
	{
		.cra_name         = "ctr(aes-kl-aml)",
		.cra_driver_name  = "ctr-aes-kl-aml",
		.cra_priority    = 100,
		.cra_flags      = CRYPTO_ALG_TYPE_ABLKCIPHER |
			CRYPTO_ALG_ASYNC | CRYPTO_ALG_NEED_FALLBACK,
		.cra_blocksize  = AES_BLOCK_SIZE,
		.cra_ctxsize    = sizeof(struct aml_aes_ctx),
		.cra_alignmask  = 0xf,
		.cra_type       = &crypto_ablkcipher_type,
		.cra_module     = THIS_MODULE,
		.cra_init       = aml_aes_lite_cra_init,
		.cra_exit       = aml_aes_lite_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    AES_MIN_KEY_SIZE,
			.max_keysize	=    AES_MAX_KEY_SIZE,
			.ivsize		=    AES_BLOCK_SIZE,
			.setkey		=    aml_aes_kl_setkey,
			.encrypt	=    aml_aes_ctr_encrypt,
			.decrypt	=    aml_aes_ctr_decrypt,
		}
	}
};

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
struct aml_aes_info aml_gxl_aes = {
	.algs = aes_algs,
	.num_algs = ARRAY_SIZE(aes_algs),
};
#endif

struct aml_aes_info aml_aes_lite = {
	.algs = aes_lite_algs,
	.num_algs = ARRAY_SIZE(aes_lite_algs),
};

#ifdef CONFIG_OF
static const struct of_device_id aml_aes_dt_match[] = {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{	.compatible = "amlogic,aes_dma",
		.data = &aml_gxl_aes,
	},
#endif
	{	.compatible = "amlogic,aes_g12a_dma",
		.data = &aml_aes_lite,
	},
	{},
};
#else
#define aml_aes_dt_match NULL
#endif

#if DMA_IRQ_MODE
static void aml_aes_queue_task(unsigned long data)
{
	struct aml_aes_dev *dd = (struct aml_aes_dev *)data;

	aml_aes_handle_queue(dd, NULL);
}

static void aml_aes_done_task(unsigned long data)
{
	struct aml_aes_dev *dd = (struct aml_aes_dev *)data;
	int err;

	err = aml_aes_crypt_dma_stop(dd);

	if (!err) {
		err = dd->flags & AES_FLAGS_ERROR;
		dd->flags = (dd->flags & ~AES_FLAGS_ERROR);
	}

	aml_dma_debug(dd->descriptor, 1, __func__, dd->thread, dd->status);

	err = dd->err ? dd->err : err;

	if (dd->total && !err) {
		if (!err)
			err = aml_aes_crypt_dma_start(dd);
		if (err == -EINPROGRESS)
			return; /* DMA started. Not fininishing. */
	}

	if (dd->ctx->kte < 0)
		err = set_aes_key_iv(dd, NULL, 0, NULL, 0);

	aml_aes_finish_req(dd, err);
	aml_aes_handle_queue(dd, NULL);
}

static irqreturn_t aml_aes_irq(int irq, void *dev_id)
{
	struct aml_aes_dev *aes_dd = dev_id;
	struct device *dev = aes_dd->dev;
	u8 status = aml_read_crypto_reg(aes_dd->status);

	if (status) {
		if (status == 0x1)
			dev_err(dev, "irq overwrite\n");
		if (aes_dd->dma->dma_busy == DMA_FLAG_MAY_OCCUPY)
			return IRQ_HANDLED;
		if ((aes_dd->flags & AES_FLAGS_DMA) &&
		    (aes_dd->dma->dma_busy & DMA_FLAG_AES_IN_USE)) {
			if (status & DMA_STATUS_KEY_ERROR)
				aes_dd->flags |= AES_FLAGS_ERROR;
			aml_write_crypto_reg(aes_dd->status, 0xf);
			aes_dd->dma->dma_busy &= ~DMA_FLAG_AES_IN_USE;
			tasklet_schedule(&aes_dd->done_task);
			return IRQ_HANDLED;
		} else {
			return IRQ_NONE;
		}
	}

	return IRQ_NONE;
}
#endif

static void aml_aes_unregister_algs(struct aml_aes_dev *dd,
				    const struct aml_aes_info *aes_info)
{
	int i;

	for (i = 0; i < aes_info->num_algs; i++)
		crypto_unregister_alg(&aes_info->algs[i]);
}

static int aml_aes_register_algs(struct aml_aes_dev *dd,
				 const struct aml_aes_info *aes_info)
{
	int err, i, j;

	for (i = 0; i < aes_info->num_algs; i++) {
		err = crypto_register_alg(&aes_info->algs[i]);
		if (err)
			goto err_aes_algs;
	}

	return 0;

err_aes_algs:
	for (j = 0; j < i; j++)
		crypto_unregister_alg(&aes_info->algs[j]);

	return err;
}

static int aml_aes_probe(struct platform_device *pdev)
{
	struct aml_aes_dev *aes_dd;
	struct device *dev = &pdev->dev;
	const struct of_device_id *match;
	int err = -EPERM;
	const struct aml_aes_info *aes_info = NULL;
	/* Set default iv_swap to 1 for backward compatible.
	 * It can be modified by specifying iv_swap in dts.
	 */
	u8 iv_swap = 1;
	u8 set_key_iv_separately = 0;

	aes_dd = devm_kzalloc(dev, sizeof(struct aml_aes_dev), GFP_KERNEL);
	if (!aes_dd) {
		err = -ENOMEM;
		goto aes_dd_err;
	}

	match = of_match_device(aml_aes_dt_match, &pdev->dev);
	if (!match) {
		dev_err(dev, "%s: cannot find match dt\n", __func__);
		err = -EINVAL;
		goto aes_dd_err;
	}

	of_property_read_u8(pdev->dev.of_node, "iv_swap", &iv_swap);
	of_property_read_u8(pdev->dev.parent->of_node, "set_key_iv_separately",
			&set_key_iv_separately);

	aes_info = match->data;
	aes_dd->dev = dev;
	aes_dd->parent = dev->parent;
	aes_dd->dma = dev_get_drvdata(dev->parent);
	aes_dd->thread = aes_dd->dma->thread;
	aes_dd->status = aes_dd->dma->status;
	aes_dd->link_mode = aes_dd->dma->link_mode;
	aes_dd->irq = aes_dd->dma->irq;
	aes_dd->iv_swap = iv_swap;
	aes_dd->set_key_iv_separately = set_key_iv_separately;
	platform_set_drvdata(pdev, aes_dd);

	INIT_LIST_HEAD(&aes_dd->list);

#if DMA_IRQ_MODE
	tasklet_init(&aes_dd->done_task, aml_aes_done_task,
		     (unsigned long)aes_dd);
	tasklet_init(&aes_dd->queue_task, aml_aes_queue_task,
		     (unsigned long)aes_dd);

	crypto_init_queue(&aes_dd->queue, AML_AES_QUEUE_LENGTH);
	err = devm_request_irq(dev, aes_dd->irq, aml_aes_irq, IRQF_SHARED,
			       "aml-aes", aes_dd);
	if (err) {
		dev_err(dev, "unable to request aes irq.\n");
		goto aes_irq_err;
	}
#endif
	pm_runtime_use_autosuspend(dev);
	pm_runtime_set_autosuspend_delay(dev, DEFAULT_AUTOSUSPEND_DELAY);
	pm_runtime_enable(dev);

	err = pm_runtime_get_sync(dev);
	if (err < 0) {
		dev_err(dev, "%s: pm_runtime_get_sync fails: %d\n",
			__func__, err);
		goto err_aes_pm;
	}

	err = aml_aes_hw_init(aes_dd);
	if (err)
		goto err_aes_buff;

	err = aml_aes_buff_init(aes_dd);
	if (err)
		goto err_aes_buff;

	spin_lock(&aml_aes.lock);
	list_add_tail(&aes_dd->list, &aml_aes.dev_list);
	spin_unlock(&aml_aes.lock);

	err = aml_aes_register_algs(aes_dd, aes_info);
	if (err)
		goto err_algs;

	dev_info(dev, "Aml AES_dma\n");

	pm_runtime_put_sync_autosuspend(dev);

	return 0;

err_algs:
	spin_lock(&aml_aes.lock);
	list_del(&aes_dd->list);
	spin_unlock(&aml_aes.lock);
	aml_aes_buff_cleanup(aes_dd);
err_aes_buff:
err_aes_pm:
	pm_runtime_disable(dev);
#if DMA_IRQ_MODE
aes_irq_err:
	tasklet_kill(&aes_dd->done_task);
	tasklet_kill(&aes_dd->queue_task);
#endif
aes_dd_err:
	dev_err(dev, "initialization failed.\n");

	return err;
}

static int aml_aes_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	static struct aml_aes_dev *aes_dd;
	const struct of_device_id *match;
	const struct aml_aes_info *aes_info = NULL;

	aes_dd = platform_get_drvdata(pdev);
	if (!aes_dd)
		return -ENODEV;
	match = of_match_device(aml_aes_dt_match, &pdev->dev);
	if (!match) {
		dev_err(dev, "%s: cannot find match dt\n", __func__);
		return -EINVAL;
	}
	aes_info = match->data;
	spin_lock(&aml_aes.lock);
	list_del(&aes_dd->list);
	spin_unlock(&aml_aes.lock);
	aml_aes_buff_cleanup(aes_dd);

	aml_aes_unregister_algs(aes_dd, aes_info);
#if DMA_IRQ_MODE
	tasklet_kill(&aes_dd->done_task);
	tasklet_kill(&aes_dd->queue_task);
#endif
	pm_runtime_disable(aes_dd->parent);
	return 0;
}

static struct platform_driver aml_aes_driver = {
	.probe		= aml_aes_probe,
	.remove		= aml_aes_remove,
	.driver		= {
		.name	= "aml_aes_dma",
		.owner	= THIS_MODULE,
		.of_match_table = aml_aes_dt_match,
	},
};

int __init aml_aes_driver_init(void)
{
	return platform_driver_register(&aml_aes_driver);
}

void aml_aes_driver_exit(void)
{
	platform_driver_unregister(&aml_aes_driver);
}
