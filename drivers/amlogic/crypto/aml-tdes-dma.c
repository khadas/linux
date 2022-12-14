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
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

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
#include <crypto/internal/des.h>
#include <crypto/scatterwalk.h>
#include <crypto/algapi.h>
#include <crypto/des.h>
#include <crypto/hash.h>
#include <crypto/internal/hash.h>
//#include <linux/amlogic/iomap.h>
#include <crypto/skcipher.h>
#include "aml-crypto-dma.h"

/* TDES flags */
#define TDES_FLAGS_MODE_MASK	0x03
#define TDES_FLAGS_ENCRYPT	BIT(0)
#define TDES_FLAGS_CBC		BIT(1)

#define TDES_FLAGS_INIT         BIT(8)
#define TDES_FLAGS_DMA          BIT(9)
#define TDES_FLAGS_FAST         BIT(10)
#define TDES_FLAGS_BUSY         BIT(11)
#define TDES_FLAGS_ERROR        BIT(12)

#define AML_TDES_QUEUE_LENGTH	50

#define DEFAULT_AUTOSUSPEND_DELAY	1000

struct aml_tdes_dev;

struct aml_tdes_ctx {
	struct aml_tdes_dev *dd;

	u32	keylen;
	u32	key[3 * DES_KEY_SIZE / sizeof(u32)];

	u16	block_size;
	struct crypto_skcipher	*fallback;
	u16 same_key;

	int kte;
};

struct aml_tdes_reqctx {
	unsigned long mode;
};

struct aml_tdes_info {
	struct crypto_alg *algs;
	u32 num_algs;
};

struct aml_tdes_dev {
	struct list_head	list;

	struct aml_tdes_ctx	*ctx;
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

	struct aml_tdes_info *info;
	u8  set_key_iv_separately;
};

struct aml_tdes_drv {
	struct list_head	dev_list;
	spinlock_t		lock; /* spinlock for device list */
};

static struct aml_tdes_drv aml_tdes = {
	.dev_list = LIST_HEAD_INIT(aml_tdes.dev_list),
	.lock = __SPIN_LOCK_UNLOCKED(aml_tdes.lock),
};

static int aml_tdes_crypt_dma_stop(struct aml_tdes_dev *dd);
static int set_tdes_kl_key_iv(struct aml_tdes_dev *dd,
			      u32 *key, u32 keylen, u32 *iv)
{
	struct dma_dsc *dsc = dd->descriptor;
	struct device *dev = dd->dev;
	u32 *key_iv = kzalloc(DMA_KEY_IV_BUF_SIZE, GFP_ATOMIC);
	u32 *piv = key_iv; // + 8;
	u32 len = keylen;
	dma_addr_t dma_addr_key;

	if (!key_iv) {
		dev_err(dev, "error allocating key_iv buffer\n");
		return -EINVAL;
	}

	if (iv)
		memcpy(piv, iv, 8);

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
	/* Internal key_iv storage of DMA is 48 bytes (32 for key; 16 for iv)
	 * For some reason, it fails to set key if key length is 8 bytes(ex: DES).
	 * Therefore, we always set length to 32 bytes(full key storage) regardless of
	 * real key length.
	 * PLEASE ask Qian Cheng(cheng.qian@amlogic.com) for details.
	 */
	dsc[0].dsc_cfg.b.length = 32;
	dsc[0].dsc_cfg.b.mode = MODE_KEY;
	dsc[0].dsc_cfg.b.owner = 1;
	dsc[0].dsc_cfg.b.eoc = 0;

	if (iv) {
		dsc[1].src_addr = (u32)dma_addr_key;
		dsc[1].tgt_addr = 32;
		dsc[1].dsc_cfg.d32 = 0;
		dsc[1].dsc_cfg.b.length = 16;
		dsc[1].dsc_cfg.b.mode = MODE_KEY;
		dsc[1].dsc_cfg.b.owner = 1;
		dsc[1].dsc_cfg.b.eoc = 1;
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

static int set_tdes_key_iv(struct aml_tdes_dev *dd,
			   u32 *key, u32 keylen, u32 *iv)
{
	struct dma_dsc *dsc = dd->descriptor;
	struct device *dev = dd->dev;
	u32 *key_iv = kzalloc(DMA_KEY_IV_BUF_SIZE, GFP_ATOMIC);
	u32 *piv = key_iv + 8;
	dma_addr_t dma_addr_key;
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

	if (iv)
		memcpy(piv, iv, 8);

	dma_addr_key = dma_map_single(dd->parent, key_iv,
				      DMA_KEY_IV_BUF_SIZE,
				      DMA_TO_DEVICE);

	if (dma_mapping_error(dd->parent, dma_addr_key)) {
		dev_err(dev, "error mapping dma_addr_key\n");
		kfree(key_iv);
		return -EINVAL;
	}

	if (!dd->set_key_iv_separately) {
		dsc[0].src_addr = (u32)dma_addr_key;
		dsc[0].tgt_addr = 0;
		dsc[0].dsc_cfg.d32 = 0;
		dsc[0].dsc_cfg.b.length = DMA_KEY_IV_BUF_SIZE;
		dsc[0].dsc_cfg.b.mode = MODE_KEY;
		dsc[0].dsc_cfg.b.owner = 1;
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
			/* If there is no iv, mark last dsc as EOC */
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
			     1, DMA_FLAG_TDES_IN_USE);
	aml_dma_debug(dsc, num_of_dsc, "end tdes keyiv", dd->thread, dd->status);
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

static size_t aml_tdes_sg_copy(struct scatterlist **sg, size_t *offset,
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

static size_t aml_tdes_sg_dma(struct aml_tdes_dev *dd, struct dma_dsc *dsc,
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

static struct aml_tdes_dev *aml_tdes_find_dev(struct aml_tdes_ctx *ctx)
{
	struct aml_tdes_dev *tdes_dd = NULL;
	struct aml_tdes_dev *tmp;

	spin_lock_bh(&aml_tdes.lock);
	if (!ctx->dd) {
		list_for_each_entry(tmp, &aml_tdes.dev_list, list) {
			tdes_dd = tmp;
			break;
		}
		ctx->dd = tdes_dd;
	} else {
		tdes_dd = ctx->dd;
	}

	spin_unlock_bh(&aml_tdes.lock);

	return tdes_dd;
}

static int aml_tdes_hw_init(struct aml_tdes_dev *dd)
{
	if (!(dd->flags & TDES_FLAGS_INIT)) {
		dd->flags |= TDES_FLAGS_INIT;
		dd->err = 0;
	}

	return 0;
}

static void aml_tdes_finish_req(struct aml_tdes_dev *dd, int err)
{
#if DMA_IRQ_MODE
	struct ablkcipher_request *req = dd->req;
#endif
	unsigned long flags;

	spin_lock_irqsave(&dd->dma->dma_lock, flags);
	dd->flags &= ~TDES_FLAGS_BUSY;
	dd->dma->dma_busy &= ~DMA_FLAG_MAY_OCCUPY;
	spin_unlock_irqrestore(&dd->dma->dma_lock, flags);
#if DMA_IRQ_MODE
	req->base.complete(&req->base, err);
#endif
}

static int aml_tdes_crypt_dma(struct aml_tdes_dev *dd, struct dma_dsc *dsc,
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

	if (dd->flags & TDES_FLAGS_CBC)
		op_mode = OP_MODE_CBC;

	for (i = 0; i < nents; i++) {
		dsc[i].dsc_cfg.b.enc_sha_only = dd->flags & TDES_FLAGS_ENCRYPT;
		dsc[i].dsc_cfg.b.mode =
			((dd->ctx->keylen == DES_KEY_SIZE) ? MODE_DES :
			 ((dd->ctx->keylen == 2 * DES_KEY_SIZE) ?
			  MODE_TDES_2K : MODE_TDES_3K));
		dsc[i].dsc_cfg.b.op_mode = op_mode;
		dsc[i].dsc_cfg.b.eoc = (i == (nents - 1));
		dsc[i].dsc_cfg.b.owner = 1;
	}

	aml_dma_debug(dsc, nents, __func__, dd->thread, dd->status);

	/* Start DMA transfer */
#if DMA_IRQ_MODE
	spin_lock_irqsave(&dd->dma->dma_lock, flags);
	dd->dma->dma_busy |= DMA_FLAG_TDES_IN_USE;
	spin_unlock_irqrestore(&dd->dma->dma_lock, flags);

	dd->flags |= TDES_FLAGS_DMA;

	aml_write_crypto_reg(dd->thread,
			     (uintptr_t)dd->dma_descript_tab | 2);
	return -EINPROGRESS;
#else
	dd->flags |= TDES_FLAGS_DMA;
	status = aml_dma_do_hw_crypto(dd->dma, dsc, nents, dd->dma_descript_tab,
			     1, DMA_FLAG_TDES_IN_USE);
	if (status & DMA_STATUS_KEY_ERROR)
		dd->flags |= TDES_FLAGS_ERROR;
	aml_dma_debug(dsc, nents, "end tdes", dd->thread, dd->status);
	err = aml_tdes_crypt_dma_stop(dd);
	if (!err) {
		err = dd->flags & TDES_FLAGS_ERROR;
		dd->flags = (dd->flags & ~TDES_FLAGS_ERROR);
	}

	return err;
#endif
}

static int aml_tdes_crypt_dma_link_mode_start(struct aml_tdes_dev *dd)
{
	int err = 0;
	size_t count = 0;
	struct dma_dsc *dsc = dd->descriptor;

	count = aml_tdes_sg_dma(dd, dsc, dd->total);
	dd->flags |= TDES_FLAGS_FAST;
	dd->fast_total = count;
	dd->total -= dd->fast_total;

	/* install IV */
	if ((dd->flags & TDES_FLAGS_CBC) && !(dd->flags & TDES_FLAGS_ENCRYPT))
		scatterwalk_map_and_copy(dd->req->info,
					 dd->in_sg,
					 dd->fast_total - DES_BLOCK_SIZE,
					 DES_BLOCK_SIZE, 0);

	/* using 1 entry in link mode */
	err = aml_tdes_crypt_dma(dd, dsc, 1);

	return err;
}

static int aml_tdes_crypt_dma_start(struct aml_tdes_dev *dd)
{
	int err = 0;
	size_t count = 0;
	dma_addr_t addr_in, addr_out;
	struct dma_dsc *dsc = dd->descriptor;
	u32 nents;

	/* slow dma */
	/* use cache buffers */
	count = aml_tdes_sg_copy(&dd->in_sg, &dd->in_offset,
				 dd->buf_in, dd->buflen, dd->total, 0);
	addr_in = dd->dma_addr_in;
	addr_out = dd->dma_addr_out;
	dd->dma_size = count;
	dma_sync_single_for_device(dd->parent, addr_in, dd->dma_size,
				   DMA_TO_DEVICE);
	dsc->src_addr = (u32)addr_in;
	dsc->tgt_addr = (u32)addr_out;
	dsc->dsc_cfg.d32 = 0;
	dsc->dsc_cfg.b.length = count;
	nents = 1;
	dd->flags &= ~TDES_FLAGS_FAST;

	dd->total -= count;
	err = aml_tdes_crypt_dma(dd, dsc, nents);

	return err;
}

static int aml_tdes_write_ctrl(struct aml_tdes_dev *dd)
{
	int err = 0;
	u32 *iv = NULL;

	err = aml_tdes_hw_init(dd);

	if (err)
		return err;

	if (dd->flags & TDES_FLAGS_CBC)
		iv = dd->req->info;

	if (dd->ctx->kte >= 0)
		err = set_tdes_kl_key_iv(dd, dd->ctx->key, dd->ctx->keylen, iv);
	else
		err = set_tdes_key_iv(dd, dd->ctx->key, dd->ctx->keylen, iv);

	return err;
}

static int aml_tdes_handle_queue(struct aml_tdes_dev *dd,
				 struct ablkcipher_request *req)
{
#if DMA_IRQ_MODE
	struct crypto_async_request *async_req, *backlog;
#endif
	struct aml_tdes_ctx *ctx;
	struct aml_tdes_reqctx *rctx;
	s32 err = 0;
#if DMA_IRQ_MODE
	s32 ret = 0;
#endif

#if DMA_IRQ_MODE
	unsigned long flags;
	spin_lock_irqsave(&dd->dma->dma_lock, flags);
	if (req)
		ret = ablkcipher_enqueue_request(&dd->queue, req);

	if ((dd->flags & TDES_FLAGS_BUSY) || dd->dma->dma_busy) {
		spin_unlock_irqrestore(&dd->dma->dma_lock, flags);
		return ret;
	}
	backlog = crypto_get_backlog(&dd->queue);
	async_req = crypto_dequeue_request(&dd->queue);
	if (async_req) {
		dd->flags |= TDES_FLAGS_BUSY;
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
	rctx->mode &= TDES_FLAGS_MODE_MASK;
	dd->flags = (dd->flags & ~TDES_FLAGS_MODE_MASK) | rctx->mode;
	dd->flags = (dd->flags & ~TDES_FLAGS_ERROR);
	dd->ctx = ctx;
	ctx->dd = dd;

	err = aml_tdes_write_ctrl(dd);
	if (!err) {
		if (dd->link_mode && !disable_link_mode) {
			err = aml_tdes_crypt_dma_link_mode_start(dd);
		} else {
			do {
				err = aml_tdes_crypt_dma_start(dd);
			} while (!err && dd->total);
		}
	}
	if (err != -EINPROGRESS) {
		/* tdes_task will not finish it, so do it here */
		aml_tdes_finish_req(dd, err);
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

int aml_tdes_process(struct ablkcipher_request *req)
{
	struct aml_tdes_ctx *tctx = crypto_tfm_ctx(req->base.tfm);
	struct aml_tdes_dev *dd = tctx->dd;

	return aml_tdes_handle_queue(dd, req);
}

static int aml_tdes_crypt_dma_stop(struct aml_tdes_dev *dd)
{
	struct device *dev = dd->dev;
	int err = -EINVAL;
	size_t count;
	u32 in_nents = 0, out_nents = 0;

	if (dd->flags & TDES_FLAGS_DMA) {
		err = 0;
		if  (dd->flags & TDES_FLAGS_FAST) {
			in_nents = sg_nents(dd->in_sg);
			out_nents = sg_nents(dd->out_sg);
			if (dd->in_sg != dd->out_sg) {
				dma_unmap_sg(dd->parent, dd->in_sg, in_nents,
						DMA_TO_DEVICE);
				dma_unmap_sg(dd->parent, dd->out_sg, out_nents,
						DMA_FROM_DEVICE);
			} else {
				dma_sync_sg_for_cpu(dd->parent, dd->in_sg,
						    in_nents, DMA_FROM_DEVICE);
				dma_unmap_sg(dd->parent, dd->in_sg,
					     in_nents, DMA_BIDIRECTIONAL);
			}

			aml_dma_link_debug(dd->sg_dsc_in, dd->dma_sg_dsc_in, in_nents, __func__);
			aml_dma_link_debug(dd->sg_dsc_out, dd->dma_sg_dsc_out, in_nents, __func__);

			dma_free_coherent(dd->parent, in_nents * sizeof(struct dma_sg_dsc),
					dd->sg_dsc_in, dd->dma_sg_dsc_in);
			dma_free_coherent(dd->parent, out_nents * sizeof(struct dma_sg_dsc),
					dd->sg_dsc_out, dd->dma_sg_dsc_out);
			/* install IV */
			if (dd->flags & TDES_FLAGS_CBC) {
				u32 length = dd->fast_total - DES_BLOCK_SIZE;

				if (dd->flags & TDES_FLAGS_ENCRYPT) {
					scatterwalk_map_and_copy(dd->req->info,
								 dd->out_sg,
								 length,
								 DES_BLOCK_SIZE,
								 0);
				}
			}
		} else {
			dma_sync_single_for_cpu(dd->parent, dd->dma_addr_out,
						dd->dma_size, DMA_FROM_DEVICE);

			/* copy data */
			count = aml_tdes_sg_copy(&dd->out_sg, &dd->out_offset,
						 dd->buf_out, dd->buflen,
						 dd->dma_size, 1);
			if (count != dd->dma_size) {
				err = -EINVAL;
				dev_err(dev, "not all data converted: %zu\n",
					count);
			}
			/* install IV */
			if (dd->flags & TDES_FLAGS_CBC) {
				if (dd->flags & TDES_FLAGS_ENCRYPT) {
					memcpy(dd->req->info, dd->buf_out +
					       dd->dma_size - DES_BLOCK_SIZE,
					       DES_BLOCK_SIZE);
				} else {
					memcpy(dd->req->info, dd->buf_in +
					       dd->dma_size - DES_BLOCK_SIZE,
					       DES_BLOCK_SIZE);
				}
			}
		}
		dd->flags &= ~TDES_FLAGS_DMA;
	}

	return err;
}

static int aml_tdes_buff_init(struct aml_tdes_dev *dd)
{
	struct device *dev = dd->dev;
	int err = -ENOMEM;

	dd->buf_in = (void *)__get_free_pages(GFP_KERNEL, 0);
	dd->buf_out = (void *)__get_free_pages(GFP_KERNEL, 0);
	dd->buflen = PAGE_SIZE;
	dd->buflen &= ~(DES_BLOCK_SIZE - 1);

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

static void aml_tdes_buff_cleanup(struct aml_tdes_dev *dd)
{
	dma_unmap_single(dd->parent, dd->dma_addr_out, dd->buflen,
			 DMA_FROM_DEVICE);
	dma_unmap_single(dd->parent, dd->dma_addr_in, dd->buflen,
			 DMA_TO_DEVICE);
	dmam_free_coherent(dd->parent, MAX_NUM_TABLES * sizeof(struct dma_dsc),
			dd->descriptor, dd->dma_descript_tab);
	free_page((unsigned long)dd->buf_out);
	free_page((unsigned long)dd->buf_in);
}

static int aml_tdes_crypt(struct ablkcipher_request *req, unsigned long mode)
{
	struct aml_tdes_ctx *ctx =
		crypto_ablkcipher_ctx(crypto_ablkcipher_reqtfm(req));
	struct aml_tdes_reqctx *rctx = ablkcipher_request_ctx(req);
	struct aml_tdes_dev *dd;
	int err;

	if (!IS_ALIGNED(req->nbytes, DES_BLOCK_SIZE)) {
		pr_err("request size is not exact amount of TDES blocks\n");
		return -EINVAL;
	}
	ctx->block_size = DES_BLOCK_SIZE;

	if (ctx->fallback && ctx->same_key) {
		struct skcipher_request *subreq = NULL;
		int ret = 0;

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

		if (mode & TDES_FLAGS_ENCRYPT)
			ret = crypto_skcipher_encrypt(subreq);
		else
			ret = crypto_skcipher_decrypt(subreq);

		skcipher_request_free(subreq);
		return ret;
	}

	dd = aml_tdes_find_dev(ctx);
	if (!dd)
		return -ENODEV;

	rctx->mode = mode;

	if (pm_runtime_suspended(dd->parent)) {
		err = pm_runtime_get_sync(dd->parent);
		if (err < 0) {
			dev_err(dd->dev, "%s: pm_runtime_get_sync fails: %d\n",
				__func__, err);
			return err;
		}
	}
#if DMA_IRQ_MODE
	return aml_tdes_handle_queue(dd, req);
#else
	return aml_dma_crypto_enqueue_req(dd->dma, &req->base);
#endif
}

static int aml_des_setkey(struct crypto_ablkcipher *tfm, const u8 *key,
			  u32 keylen)
{
	int err;
	struct crypto_tfm *ctfm = crypto_ablkcipher_tfm(tfm);
	struct aml_tdes_ctx *ctx = crypto_ablkcipher_ctx(tfm);

	if (keylen != DES_KEY_SIZE) {
		crypto_ablkcipher_set_flags(tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);
		return -EINVAL;
	}

	err = crypto_des_verify_key(ctfm, key);
	if (err != 0  && (ctfm->crt_flags & CRYPTO_TFM_REQ_FORBID_WEAK_KEYS)) {
		ctfm->crt_flags |= CRYPTO_TFM_RES_WEAK_KEY;
		return -EINVAL;
	}

	memcpy(ctx->key, key, keylen);
	ctx->keylen = keylen;
	ctx->kte = -1;

	return 0;
}

static int aml_des_kl_setkey(struct crypto_ablkcipher *tfm, const u8 *key,
			     u32 keylen)
{
	struct aml_tdes_ctx *ctx = crypto_ablkcipher_ctx(tfm);

	if (keylen != DES_KEY_SIZE) {
		crypto_ablkcipher_set_flags(tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);
		return -EINVAL;
	}

	/* key[0:3] = kte */
	ctx->kte = *(uint32_t *)&key[0];
	ctx->keylen = keylen;

	return 0;
}

/*
static int aml_tdes_setkey(struct crypto_ablkcipher *tfm, const u8 *key,
			   u32 keylen)
{
	struct aml_tdes_ctx *ctx = crypto_ablkcipher_ctx(tfm);

	if ((keylen != 2 * DES_KEY_SIZE) && (keylen != 3 * DES_KEY_SIZE)) {
		crypto_ablkcipher_set_flags(tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);
		return -EINVAL;
	}

	memcpy(ctx->key, key, keylen);
	ctx->keylen = keylen;
	ctx->kte = -1;

	return 0;
}
*/

static int aml_tdes_ecb_encrypt(struct ablkcipher_request *req)
{
	return aml_tdes_crypt(req,
			TDES_FLAGS_ENCRYPT);
}

static int aml_tdes_ecb_decrypt(struct ablkcipher_request *req)
{
	return aml_tdes_crypt(req,
			0);
}

static int aml_tdes_cbc_encrypt(struct ablkcipher_request *req)
{
	return aml_tdes_crypt(req,
			TDES_FLAGS_ENCRYPT | TDES_FLAGS_CBC);
}

static int aml_tdes_cbc_decrypt(struct ablkcipher_request *req)
{
	return aml_tdes_crypt(req,
			TDES_FLAGS_CBC);
}

/*
static int aml_tdes_cra_init(struct crypto_tfm *tfm)
{
	struct aml_tdes_ctx *ctx = crypto_tfm_ctx(tfm);

	tfm->crt_ablkcipher.reqsize = sizeof(struct aml_tdes_reqctx);
	ctx->fallback = NULL;

	return 0;
}

static void aml_tdes_cra_exit(struct crypto_tfm *tfm)
{
}
*/

static int aml_tdes_lite_cra_init(struct crypto_tfm *tfm);
static void aml_tdes_lite_cra_exit(struct crypto_tfm *tfm);
static int aml_tdes_lite_setkey(struct crypto_ablkcipher *tfm, const u8 *key,
				unsigned int keylen);
static int aml_tdes_kl_setkey(struct crypto_ablkcipher *tfm, const u8 *key,
			      unsigned int keylen);
static struct crypto_alg des_tdes_algs[] = {
	{
		.cra_name        = "ecb(des-aml)",
		.cra_driver_name = "ecb-des-aml",
		.cra_priority  =  100,
		.cra_flags     =  CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize =  DES_BLOCK_SIZE,
		.cra_ctxsize   =  sizeof(struct aml_tdes_ctx),
		.cra_alignmask =  0,
		.cra_type      =  &crypto_ablkcipher_type,
		.cra_module    =  THIS_MODULE,
		.cra_init      =  aml_tdes_lite_cra_init,
		.cra_exit      =  aml_tdes_lite_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    DES_KEY_SIZE,
			.max_keysize	=    DES_KEY_SIZE,
			.setkey		=    aml_des_setkey,
			.encrypt	=    aml_tdes_ecb_encrypt,
			.decrypt	=    aml_tdes_ecb_decrypt,
		}
	},
	{
		.cra_name        =  "cbc(des-aml)",
		.cra_driver_name =  "cbc-des-aml",
		.cra_priority  =  100,
		.cra_flags     =  CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize =  DES_BLOCK_SIZE,
		.cra_ctxsize   =  sizeof(struct aml_tdes_ctx),
		.cra_alignmask =  0,
		.cra_type      =  &crypto_ablkcipher_type,
		.cra_module    =  THIS_MODULE,
		.cra_init      =  aml_tdes_lite_cra_init,
		.cra_exit      =  aml_tdes_lite_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    DES_KEY_SIZE,
			.max_keysize	=    DES_KEY_SIZE,
			.ivsize		=    DES_BLOCK_SIZE,
			.setkey		=    aml_des_setkey,
			.encrypt	=    aml_tdes_cbc_encrypt,
			.decrypt	=    aml_tdes_cbc_decrypt,
		}
	},
	{
		.cra_name        = "ecb(des3_ede-aml)",
		.cra_driver_name = "ecb-tdes-aml",
		.cra_priority   = 100,
		.cra_flags      = CRYPTO_ALG_TYPE_ABLKCIPHER |
			CRYPTO_ALG_ASYNC | CRYPTO_ALG_NEED_FALLBACK,
		.cra_blocksize  = DES_BLOCK_SIZE,
		.cra_ctxsize    = sizeof(struct aml_tdes_ctx),
		.cra_alignmask  = 0,
		.cra_type       = &crypto_ablkcipher_type,
		.cra_module     = THIS_MODULE,
		.cra_init       = aml_tdes_lite_cra_init,
		.cra_exit       = aml_tdes_lite_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    2 * DES_KEY_SIZE,
			.max_keysize	=    3 * DES_KEY_SIZE,
			.setkey		=    aml_tdes_lite_setkey,
			.encrypt	=    aml_tdes_ecb_encrypt,
			.decrypt	=    aml_tdes_ecb_decrypt,
		}
	},
	{
		.cra_name        = "cbc(des3_ede-aml)",
		.cra_driver_name = "cbc-tdes-aml",
		.cra_priority  = 100,
		.cra_flags      = CRYPTO_ALG_TYPE_ABLKCIPHER |
			CRYPTO_ALG_ASYNC | CRYPTO_ALG_NEED_FALLBACK,
		.cra_blocksize = DES_BLOCK_SIZE,
		.cra_ctxsize   = sizeof(struct aml_tdes_ctx),
		.cra_alignmask = 0,
		.cra_type      = &crypto_ablkcipher_type,
		.cra_module    = THIS_MODULE,
		.cra_init      = aml_tdes_lite_cra_init,
		.cra_exit      = aml_tdes_lite_cra_exit,
		.cra_u.ablkcipher =       {
			.min_keysize = 2 * DES_KEY_SIZE,
			.max_keysize = 3 * DES_KEY_SIZE,
			.ivsize	     = DES_BLOCK_SIZE,
			.setkey	     = aml_tdes_lite_setkey,
			.encrypt     = aml_tdes_cbc_encrypt,
			.decrypt     = aml_tdes_cbc_decrypt,
		}
	},
	{
		.cra_name        = "ecb(des-kl-aml)",
		.cra_driver_name = "ecb-des-kl-aml",
		.cra_priority  =  100,
		.cra_flags     =  CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize =  DES_BLOCK_SIZE,
		.cra_ctxsize   =  sizeof(struct aml_tdes_ctx),
		.cra_alignmask =  0,
		.cra_type      =  &crypto_ablkcipher_type,
		.cra_module    =  THIS_MODULE,
		.cra_init      =  aml_tdes_lite_cra_init,
		.cra_exit      =  aml_tdes_lite_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    DES_KEY_SIZE,
			.max_keysize	=    DES_KEY_SIZE,
			.setkey		=    aml_des_kl_setkey,
			.encrypt	=    aml_tdes_ecb_encrypt,
			.decrypt	=    aml_tdes_ecb_decrypt,
		}
	},
	{
		.cra_name        =  "cbc(des-kl-aml)",
		.cra_driver_name =  "cbc-des-kl-aml",
		.cra_priority  =  100,
		.cra_flags     =  CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize =  DES_BLOCK_SIZE,
		.cra_ctxsize   =  sizeof(struct aml_tdes_ctx),
		.cra_alignmask =  0,
		.cra_type      =  &crypto_ablkcipher_type,
		.cra_module    =  THIS_MODULE,
		.cra_init      =  aml_tdes_lite_cra_init,
		.cra_exit      =  aml_tdes_lite_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    DES_KEY_SIZE,
			.max_keysize	=    DES_KEY_SIZE,
			.ivsize		=    DES_BLOCK_SIZE,
			.setkey		=    aml_des_kl_setkey,
			.encrypt	=    aml_tdes_cbc_encrypt,
			.decrypt	=    aml_tdes_cbc_decrypt,
		}
	},
	{
		.cra_name        = "ecb(tdes-kl-aml)",
		.cra_driver_name = "ecb-tdes-kl-aml",
		.cra_priority   = 100,
		.cra_flags      = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize  = DES_BLOCK_SIZE,
		.cra_ctxsize    = sizeof(struct aml_tdes_ctx),
		.cra_alignmask  = 0,
		.cra_type       = &crypto_ablkcipher_type,
		.cra_module     = THIS_MODULE,
		.cra_init       = aml_tdes_lite_cra_init,
		.cra_exit       = aml_tdes_lite_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    2 * DES_KEY_SIZE,
			.max_keysize	=    3 * DES_KEY_SIZE,
			.setkey     =    aml_tdes_kl_setkey,
			.encrypt    =    aml_tdes_ecb_encrypt,
			.decrypt    =    aml_tdes_ecb_decrypt,
		}
	},
	{
		.cra_name        = "cbc(tdes-kl-aml)",
		.cra_driver_name = "cbc-tdes-kl-aml",
		.cra_priority  = 100,
		.cra_flags     = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize = DES_BLOCK_SIZE,
		.cra_ctxsize   = sizeof(struct aml_tdes_ctx),
		.cra_alignmask = 0,
		.cra_type      = &crypto_ablkcipher_type,
		.cra_module    = THIS_MODULE,
		.cra_init      = aml_tdes_lite_cra_init,
		.cra_exit      = aml_tdes_lite_cra_exit,
		.cra_u.ablkcipher =       {
			.min_keysize = 2 * DES_KEY_SIZE,
			.max_keysize = 3 * DES_KEY_SIZE,
			.ivsize	     = DES_BLOCK_SIZE,
			.setkey	     = aml_tdes_kl_setkey,
			.encrypt     = aml_tdes_cbc_encrypt,
			.decrypt     = aml_tdes_cbc_decrypt,
		}
	}
};

static struct crypto_alg des_tdes_algs_no_kl[] = {
	{
		.cra_name        = "ecb(des-aml)",
		.cra_driver_name = "ecb-des-aml",
		.cra_priority  =  100,
		.cra_flags     =  CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize =  DES_BLOCK_SIZE,
		.cra_ctxsize   =  sizeof(struct aml_tdes_ctx),
		.cra_alignmask =  0,
		.cra_type      =  &crypto_ablkcipher_type,
		.cra_module    =  THIS_MODULE,
		.cra_init      =  aml_tdes_lite_cra_init,
		.cra_exit      =  aml_tdes_lite_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    DES_KEY_SIZE,
			.max_keysize	=    DES_KEY_SIZE,
			.setkey		=    aml_des_setkey,
			.encrypt	=    aml_tdes_ecb_encrypt,
			.decrypt	=    aml_tdes_ecb_decrypt,
		}
	},
	{
		.cra_name        =  "cbc(des-aml)",
		.cra_driver_name =  "cbc-des-aml",
		.cra_priority  =  100,
		.cra_flags     =  CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize =  DES_BLOCK_SIZE,
		.cra_ctxsize   =  sizeof(struct aml_tdes_ctx),
		.cra_alignmask =  0,
		.cra_type      =  &crypto_ablkcipher_type,
		.cra_module    =  THIS_MODULE,
		.cra_init      =  aml_tdes_lite_cra_init,
		.cra_exit      =  aml_tdes_lite_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    DES_KEY_SIZE,
			.max_keysize	=    DES_KEY_SIZE,
			.ivsize		=    DES_BLOCK_SIZE,
			.setkey		=    aml_des_setkey,
			.encrypt	=    aml_tdes_cbc_encrypt,
			.decrypt	=    aml_tdes_cbc_decrypt,
		}
	},
	{
		.cra_name        = "ecb(des3_ede-aml)",
		.cra_driver_name = "ecb-tdes-aml",
		.cra_priority   = 100,
		.cra_flags      = CRYPTO_ALG_TYPE_ABLKCIPHER |
			CRYPTO_ALG_ASYNC | CRYPTO_ALG_NEED_FALLBACK,
		.cra_blocksize  = DES_BLOCK_SIZE,
		.cra_ctxsize    = sizeof(struct aml_tdes_ctx),
		.cra_alignmask  = 0,
		.cra_type       = &crypto_ablkcipher_type,
		.cra_module     = THIS_MODULE,
		.cra_init       = aml_tdes_lite_cra_init,
		.cra_exit       = aml_tdes_lite_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    2 * DES_KEY_SIZE,
			.max_keysize	=    3 * DES_KEY_SIZE,
			.setkey		=    aml_tdes_lite_setkey,
			.encrypt	=    aml_tdes_ecb_encrypt,
			.decrypt	=    aml_tdes_ecb_decrypt,
		}
	},
	{
		.cra_name        = "cbc(des3_ede-aml)",
		.cra_driver_name = "cbc-tdes-aml",
		.cra_priority  = 100,
		.cra_flags      = CRYPTO_ALG_TYPE_ABLKCIPHER |
			CRYPTO_ALG_ASYNC | CRYPTO_ALG_NEED_FALLBACK,
		.cra_blocksize = DES_BLOCK_SIZE,
		.cra_ctxsize   = sizeof(struct aml_tdes_ctx),
		.cra_alignmask = 0,
		.cra_type      = &crypto_ablkcipher_type,
		.cra_module    = THIS_MODULE,
		.cra_init      = aml_tdes_lite_cra_init,
		.cra_exit      = aml_tdes_lite_cra_exit,
		.cra_u.ablkcipher =       {
			.min_keysize = 2 * DES_KEY_SIZE,
			.max_keysize = 3 * DES_KEY_SIZE,
			.ivsize	     = DES_BLOCK_SIZE,
			.setkey	     = aml_tdes_lite_setkey,
			.encrypt     = aml_tdes_cbc_encrypt,
			.decrypt     = aml_tdes_cbc_decrypt,
		}
	},
};

static int aml_tdes_lite_cra_init(struct crypto_tfm *tfm)
{
	struct aml_tdes_ctx *ctx = crypto_tfm_ctx(tfm);
	const char *alg_name = crypto_tfm_alg_name(tfm);
	const char *driver_name = crypto_tfm_alg_driver_name(tfm);
	const u32 flags = CRYPTO_ALG_ASYNC | CRYPTO_ALG_NEED_FALLBACK;
	char alg_to_use[16] = {0};

	tfm->crt_ablkcipher.reqsize = sizeof(struct aml_tdes_reqctx);

	/* Driver name with '-kl' is only for installing key by kte.
	 * No need to allocate fallback for it
	 */
	if (!strstr(driver_name, "-kl")) {
		/* Allocate a fallback and abort if it failed. */
		strncpy(alg_to_use, "xxx(des3_ede)", sizeof(alg_to_use));
		memcpy(alg_to_use, alg_name, 3);
		ctx->fallback = crypto_alloc_skcipher(alg_to_use, 0,
						      flags);
		if (IS_ERR(ctx->fallback)) {
			pr_err("aml-tdes: fallback '%s' could not be loaded.\n",
			       alg_name);
			return PTR_ERR(ctx->fallback);
		}
	}

	return 0;
}

static void aml_tdes_lite_cra_exit(struct crypto_tfm *tfm)
{
	struct aml_tdes_ctx *ctx = crypto_tfm_ctx(tfm);

	if (ctx->fallback)
		crypto_free_skcipher(ctx->fallback);

	ctx->fallback = NULL;
}

static int aml_tdes_kl_setkey(struct crypto_ablkcipher *tfm, const u8 *key,
			      unsigned int keylen)
{
	struct aml_tdes_ctx *ctx = crypto_ablkcipher_ctx(tfm);
	int ret = 0;

	if ((keylen != 2 * DES_KEY_SIZE) && (keylen != 3 * DES_KEY_SIZE)) {
		crypto_ablkcipher_set_flags(tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);
		return -EINVAL;
	}

	/* key[0:3] = kte */
	ctx->kte = *(uint32_t *)&key[0];

	ctx->keylen = keylen;

	return ret;
}

static int aml_tdes_lite_setkey(struct crypto_ablkcipher *tfm, const u8 *key,
				unsigned int keylen)
{
	struct aml_tdes_ctx *ctx = crypto_ablkcipher_ctx(tfm);
	int ret = 0;
	u64 *tmp = NULL;

	if ((keylen != 2 * DES_KEY_SIZE) && (keylen != 3 * DES_KEY_SIZE)) {
		crypto_ablkcipher_set_flags(tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);
		return -EINVAL;
	}

	memcpy(ctx->key, key, keylen);
	ctx->keylen = keylen;
	ctx->kte = -1;

	tmp = (u64 *)ctx->key;
	if (keylen == 2 * DES_KEY_SIZE)
		ctx->same_key = !(tmp[0] ^ tmp[1]);
	else
		ctx->same_key = !((tmp[0] ^ tmp[1]) | (tmp[0] ^ tmp[2]));

	if (ctx->same_key) {
		crypto_skcipher_clear_flags(ctx->fallback, CRYPTO_TFM_REQ_MASK);
		crypto_skcipher_set_flags(ctx->fallback, tfm->base.crt_flags &
				CRYPTO_TFM_REQ_MASK);
		ret = crypto_skcipher_setkey(ctx->fallback, key, keylen);
	}

	return ret;
}

static struct crypto_alg tdes_lite_algs[] = {
	{
		.cra_name        = "ecb(des3_ede-aml)",
		.cra_driver_name = "ecb-tdes-lite-aml",
		.cra_priority   = 100,
		.cra_flags      = CRYPTO_ALG_TYPE_ABLKCIPHER |
			CRYPTO_ALG_ASYNC |  CRYPTO_ALG_NEED_FALLBACK,
		.cra_blocksize  = DES_BLOCK_SIZE,
		.cra_ctxsize    = sizeof(struct aml_tdes_ctx),
		.cra_alignmask  = 0,
		.cra_type       = &crypto_ablkcipher_type,
		.cra_module     = THIS_MODULE,
		.cra_init       = aml_tdes_lite_cra_init,
		.cra_exit       = aml_tdes_lite_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    2 * DES_KEY_SIZE,
			.max_keysize	=    3 * DES_KEY_SIZE,
			.setkey     =    aml_tdes_lite_setkey,
			.encrypt    =    aml_tdes_ecb_encrypt,
			.decrypt    =    aml_tdes_ecb_decrypt,
		}
	},
	{
		.cra_name        = "cbc(des3_ede-aml)",
		.cra_driver_name = "cbc-tdes-lite-aml",
		.cra_priority  = 100,
		.cra_flags     = CRYPTO_ALG_TYPE_ABLKCIPHER |
			CRYPTO_ALG_ASYNC | CRYPTO_ALG_NEED_FALLBACK,
		.cra_blocksize = DES_BLOCK_SIZE,
		.cra_ctxsize   = sizeof(struct aml_tdes_ctx),
		.cra_alignmask = 0,
		.cra_type      = &crypto_ablkcipher_type,
		.cra_module    = THIS_MODULE,
		.cra_init      = aml_tdes_lite_cra_init,
		.cra_exit      = aml_tdes_lite_cra_exit,
		.cra_u.ablkcipher =       {
			.min_keysize = 2 * DES_KEY_SIZE,
			.max_keysize = 3 * DES_KEY_SIZE,
			.ivsize	     = DES_BLOCK_SIZE,
			.setkey	     = aml_tdes_lite_setkey,
			.encrypt     = aml_tdes_cbc_encrypt,
			.decrypt     = aml_tdes_cbc_decrypt,
		}
	},
	{
		.cra_name        = "ecb(tdes-kl-aml)",
		.cra_driver_name = "ecb-tdes-kl-aml",
		.cra_priority   = 100,
		.cra_flags      = CRYPTO_ALG_TYPE_ABLKCIPHER |
			CRYPTO_ALG_ASYNC |  CRYPTO_ALG_NEED_FALLBACK,
		.cra_blocksize  = DES_BLOCK_SIZE,
		.cra_ctxsize    = sizeof(struct aml_tdes_ctx),
		.cra_alignmask  = 0,
		.cra_type       = &crypto_ablkcipher_type,
		.cra_module     = THIS_MODULE,
		.cra_init       = aml_tdes_lite_cra_init,
		.cra_exit       = aml_tdes_lite_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    2 * DES_KEY_SIZE,
			.max_keysize	=    3 * DES_KEY_SIZE,
			.setkey     =    aml_tdes_kl_setkey,
			.encrypt    =    aml_tdes_ecb_encrypt,
			.decrypt    =    aml_tdes_ecb_decrypt,
		}
	},
	{
		.cra_name        = "cbc(tdes-kl-aml)",
		.cra_driver_name = "cbc-tdes-kl-aml",
		.cra_priority  = 100,
		.cra_flags     = CRYPTO_ALG_TYPE_ABLKCIPHER |
			CRYPTO_ALG_ASYNC | CRYPTO_ALG_NEED_FALLBACK,
		.cra_blocksize = DES_BLOCK_SIZE,
		.cra_ctxsize   = sizeof(struct aml_tdes_ctx),
		.cra_alignmask = 0,
		.cra_type      = &crypto_ablkcipher_type,
		.cra_module    = THIS_MODULE,
		.cra_init      = aml_tdes_lite_cra_init,
		.cra_exit      = aml_tdes_lite_cra_exit,
		.cra_u.ablkcipher =       {
			.min_keysize = 2 * DES_KEY_SIZE,
			.max_keysize = 3 * DES_KEY_SIZE,
			.ivsize	     = DES_BLOCK_SIZE,
			.setkey	     = aml_tdes_kl_setkey,
			.encrypt     = aml_tdes_cbc_encrypt,
			.decrypt     = aml_tdes_cbc_decrypt,
		}
	}
};

static struct crypto_alg tdes_lite_algs_no_kl[] = {
	{
		.cra_name        = "ecb(des3_ede-aml)",
		.cra_driver_name = "ecb-tdes-lite-aml",
		.cra_priority   = 100,
		.cra_flags      = CRYPTO_ALG_TYPE_ABLKCIPHER |
			CRYPTO_ALG_ASYNC |  CRYPTO_ALG_NEED_FALLBACK,
		.cra_blocksize  = DES_BLOCK_SIZE,
		.cra_ctxsize    = sizeof(struct aml_tdes_ctx),
		.cra_alignmask  = 0,
		.cra_type       = &crypto_ablkcipher_type,
		.cra_module     = THIS_MODULE,
		.cra_init       = aml_tdes_lite_cra_init,
		.cra_exit       = aml_tdes_lite_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    2 * DES_KEY_SIZE,
			.max_keysize	=    3 * DES_KEY_SIZE,
			.setkey     =    aml_tdes_lite_setkey,
			.encrypt    =    aml_tdes_ecb_encrypt,
			.decrypt    =    aml_tdes_ecb_decrypt,
		}
	},
	{
		.cra_name        = "cbc(des3_ede-aml)",
		.cra_driver_name = "cbc-tdes-lite-aml",
		.cra_priority  = 100,
		.cra_flags     = CRYPTO_ALG_TYPE_ABLKCIPHER |
			CRYPTO_ALG_ASYNC | CRYPTO_ALG_NEED_FALLBACK,
		.cra_blocksize = DES_BLOCK_SIZE,
		.cra_ctxsize   = sizeof(struct aml_tdes_ctx),
		.cra_alignmask = 0,
		.cra_type      = &crypto_ablkcipher_type,
		.cra_module    = THIS_MODULE,
		.cra_init      = aml_tdes_lite_cra_init,
		.cra_exit      = aml_tdes_lite_cra_exit,
		.cra_u.ablkcipher =       {
			.min_keysize = 2 * DES_KEY_SIZE,
			.max_keysize = 3 * DES_KEY_SIZE,
			.ivsize	     = DES_BLOCK_SIZE,
			.setkey	     = aml_tdes_lite_setkey,
			.encrypt     = aml_tdes_cbc_encrypt,
			.decrypt     = aml_tdes_cbc_decrypt,
		}
	},
	};

#if DMA_IRQ_MODE
static void aml_tdes_queue_task(unsigned long data)
{
	struct aml_tdes_dev *dd = (struct aml_tdes_dev *)data;

	aml_tdes_handle_queue(dd, NULL);
}

static void aml_tdes_done_task(unsigned long data)
{
	struct aml_tdes_dev *dd = (struct aml_tdes_dev *)data;
	int err;

	err = aml_tdes_crypt_dma_stop(dd);

	if (!err) {
		err = dd->flags & TDES_FLAGS_ERROR;
		dd->flags = (dd->flags & ~TDES_FLAGS_ERROR);
	}

	err = dd->err ? dd->err : err;

	if (dd->total && !err) {
		if (!err)
			err = aml_tdes_crypt_dma_start(dd);
		if (err == -EINPROGRESS)
			return; /* DMA started. Not fininishing. */
	}

	if (dd->ctx->kte < 0)
		err = set_tdes_key_iv(dd, NULL, 0, NULL);

	aml_tdes_finish_req(dd, err);
	aml_tdes_handle_queue(dd, NULL);
}

static irqreturn_t aml_tdes_irq(int irq, void *dev_id)
{
	struct aml_tdes_dev *tdes_dd = dev_id;
	struct device *dev = tdes_dd->dev;
	u8 status = aml_read_crypto_reg(tdes_dd->status);

	if (status) {
		if (status == 0x1)
			dev_err(dev, "irq overwrite\n");
		if (tdes_dd->dma->dma_busy == DMA_FLAG_MAY_OCCUPY)
			return IRQ_HANDLED;
		if ((tdes_dd->dma->dma_busy & DMA_FLAG_TDES_IN_USE) &&
		    (tdes_dd->flags & TDES_FLAGS_DMA)) {
			if (status & DMA_STATUS_KEY_ERROR)
				tdes_dd->flags |= TDES_FLAGS_ERROR;
			aml_write_crypto_reg(tdes_dd->status, 0xf);
			tdes_dd->dma->dma_busy &= ~DMA_FLAG_TDES_IN_USE;
			tasklet_schedule(&tdes_dd->done_task);
			return IRQ_HANDLED;
		} else {
			return IRQ_NONE;
		}
	}

	return IRQ_NONE;
}
#endif
static void aml_tdes_unregister_algs(struct aml_tdes_dev *dd,
				     struct aml_tdes_info *tdes_info)
{
	int i;

	for (i = 0; i < tdes_info->num_algs; i++)
		crypto_unregister_alg(&tdes_info->algs[i]);
}

static int aml_tdes_register_algs(struct aml_tdes_dev *dd,
				  struct aml_tdes_info *tdes_info)
{
	int err, i, j;

	for (i = 0; i < tdes_info->num_algs; i++) {
		err = crypto_register_alg(&tdes_info->algs[i]);
		if (err)
			goto err_tdes_algs;
	}

	return 0;

err_tdes_algs:
	for (j = 0; j < i; j++)
		crypto_unregister_alg(&tdes_info->algs[j]);

	return err;
}

struct aml_tdes_info aml_des_tdes __initdata = {
	.algs = des_tdes_algs,
	.num_algs = ARRAY_SIZE(des_tdes_algs),
};

struct aml_tdes_info aml_des_tdes_no_kl __initdata = {
	.algs = des_tdes_algs_no_kl,
	.num_algs = ARRAY_SIZE(des_tdes_algs_no_kl),
};

struct aml_tdes_info aml_tdes_lite __initdata = {
	.algs = tdes_lite_algs,
	.num_algs = ARRAY_SIZE(tdes_lite_algs),
};

struct aml_tdes_info aml_tdes_lite_no_kl __initdata = {
	.algs = tdes_lite_algs_no_kl,
	.num_algs = ARRAY_SIZE(tdes_lite_algs_no_kl),
};

#ifdef CONFIG_OF
static const struct of_device_id aml_tdes_dt_match[] = {
	/* For platform supporting DES and KL */
	{	.compatible = "amlogic,des_dma,tdes_dma",
		.data = &aml_des_tdes,
	},
	/* For platform supporting DES */
	{	.compatible = "amlogic,des_dma,tdes_dma,no_kl",
		.data = &aml_des_tdes_no_kl,
	},
	/* For platform without DES */
	{	.compatible = "amlogic,tdes_dma",
		.data = &aml_tdes_lite,
	},
	/* For platform without DES and KL */
	{	.compatible = "amlogic,tdes_dma,no_kl",
		.data = &aml_tdes_lite_no_kl,
	},
	{},
};
#else
#define aml_tdes_dt_match NULL
#endif

static int aml_tdes_probe(struct platform_device *pdev)
{
	struct aml_tdes_dev *tdes_dd;
	struct device *dev = &pdev->dev;
	int err = -EPERM;
	struct aml_tdes_info *tdes_info = NULL, *match;
	u8 set_key_iv_separately = 0;

	tdes_dd = devm_kzalloc(dev, sizeof(struct aml_tdes_dev), GFP_KERNEL);
	if (!tdes_dd) {
		err = -ENOMEM;
		goto tdes_dd_err;
	}

	match = (struct aml_tdes_info *)
		of_device_get_match_data(&pdev->dev);
	if (!match) {
		dev_err(dev, "%s: cannot find match dt\n", __func__);
		err = -EINVAL;
		goto tdes_dd_err;
	}

	of_property_read_u8(pdev->dev.parent->of_node, "set_key_iv_separately",
			&set_key_iv_separately);

	tdes_info = devm_kzalloc(dev, sizeof(*tdes_info), GFP_KERNEL);
	tdes_info->algs = match->algs;
	tdes_info->num_algs = match->num_algs;
	tdes_dd->info = tdes_info;
	tdes_dd->dev = dev;
	tdes_dd->parent = dev->parent;
	tdes_dd->dma = dev_get_drvdata(dev->parent);
	tdes_dd->thread = tdes_dd->dma->thread;
	tdes_dd->status = tdes_dd->dma->status;
	tdes_dd->link_mode = tdes_dd->dma->link_mode;
	tdes_dd->irq = tdes_dd->dma->irq;
	tdes_dd->set_key_iv_separately = set_key_iv_separately;

	platform_set_drvdata(pdev, tdes_dd);

	INIT_LIST_HEAD(&tdes_dd->list);

#if DMA_IRQ_MODE
	tasklet_init(&tdes_dd->done_task, aml_tdes_done_task,
		     (unsigned long)tdes_dd);
	tasklet_init(&tdes_dd->queue_task, aml_tdes_queue_task,
		     (unsigned long)tdes_dd);

	crypto_init_queue(&tdes_dd->queue, AML_TDES_QUEUE_LENGTH);
	err = devm_request_irq(dev, tdes_dd->irq, aml_tdes_irq, IRQF_SHARED,
			       "aml-tdes", tdes_dd);
	if (err) {
		dev_err(dev, "unable to request tdes irq.\n");
		goto tdes_irq_err;
	}
#endif
	pm_runtime_use_autosuspend(dev);
	pm_runtime_set_autosuspend_delay(dev, DEFAULT_AUTOSUSPEND_DELAY);
	pm_runtime_enable(dev);

	err = pm_runtime_get_sync(dev);
	if (err < 0) {
		dev_err(dev, "%s: pm_runtime_get_sync fails: %d\n",
			__func__, err);
		goto err_tdes_pm;
	}

	err = aml_tdes_hw_init(tdes_dd);
	if (err)
		goto err_tdes_buff;

	err = aml_tdes_buff_init(tdes_dd);
	if (err)
		goto err_tdes_buff;

	spin_lock(&aml_tdes.lock);
	list_add_tail(&tdes_dd->list, &aml_tdes.dev_list);
	spin_unlock(&aml_tdes.lock);

	err = aml_tdes_register_algs(tdes_dd, tdes_info);
	if (err)
		goto err_algs;

	dev_info(dev, "Aml TDES_dma\n");

	pm_runtime_put_sync_autosuspend(dev);

	return 0;

err_algs:
	spin_lock(&aml_tdes.lock);
	list_del(&tdes_dd->list);
	spin_unlock(&aml_tdes.lock);
	aml_tdes_buff_cleanup(tdes_dd);
err_tdes_buff:
err_tdes_pm:
	pm_runtime_disable(dev);
#if DMA_IRQ_MODE
tdes_irq_err:
	tasklet_kill(&tdes_dd->done_task);
	tasklet_kill(&tdes_dd->queue_task);
#endif
tdes_dd_err:
	dev_err(dev, "initialization failed.\n");

	return err;
}

static int aml_tdes_remove(struct platform_device *pdev)
{
	static struct aml_tdes_dev *tdes_dd;
	struct aml_tdes_info *tdes_info = NULL;

	tdes_dd = platform_get_drvdata(pdev);
	if (!tdes_dd)
		return -ENODEV;

	tdes_info = tdes_dd->info;
	spin_lock(&aml_tdes.lock);
	list_del(&tdes_dd->list);
	spin_unlock(&aml_tdes.lock);

	aml_tdes_buff_cleanup(tdes_dd);
	aml_tdes_unregister_algs(tdes_dd, tdes_info);

	tasklet_kill(&tdes_dd->done_task);
	tasklet_kill(&tdes_dd->queue_task);

	pm_runtime_disable(tdes_dd->parent);

	return 0;
}

static struct platform_driver aml_tdes_driver = {
	.probe		= aml_tdes_probe,
	.remove		= aml_tdes_remove,
	.driver		= {
		.name	= "aml_tdes_dma",
		.owner	= THIS_MODULE,
		.of_match_table = aml_tdes_dt_match,
	},
};

int __init aml_tdes_driver_init(void)
{
	return platform_driver_register(&aml_tdes_driver);
}

void aml_tdes_driver_exit(void)
{
	platform_driver_unregister(&aml_tdes_driver);
}
