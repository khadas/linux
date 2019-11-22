/*
 * drivers/amlogic/crypto/aml-tdes-dma.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
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

#define AML_TDES_QUEUE_LENGTH	50

#define SUPPORT_FAST_DMA 0
struct aml_tdes_dev;

struct aml_tdes_ctx {
	struct aml_tdes_dev *dd;

	u32	keylen;
	u32	key[3*DES_KEY_SIZE / sizeof(u32)];

	u16	block_size;
	struct crypto_skcipher	*fallback;
	u16 same_key;
};

struct aml_tdes_reqctx {
	unsigned long mode;
};

struct aml_tdes_dev {
	struct list_head	list;

	struct aml_tdes_ctx	*ctx;
	struct device		*dev;
	int	irq;

	unsigned long		flags;
	int	err;

	struct aml_dma_dev      *dma;
	uint32_t thread;
	uint32_t status;
	struct crypto_queue	queue;

	struct tasklet_struct	done_task;
	struct tasklet_struct	queue_task;

	struct ablkcipher_request	*req;
	size_t	total;

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

	uint32_t fast_nents;
};

struct aml_tdes_drv {
	struct list_head	dev_list;
	spinlock_t		lock;
};

struct aml_tdes_info {
	struct crypto_alg *algs;
	uint32_t num_algs;
};

static struct aml_tdes_drv aml_tdes = {
	.dev_list = LIST_HEAD_INIT(aml_tdes.dev_list),
	.lock = __SPIN_LOCK_UNLOCKED(aml_tdes.lock),
};

static int set_tdes_key_iv(struct aml_tdes_dev *dd,
		u32 *key, u32 keylen, u32 *iv)
{
	struct dma_dsc *dsc = dd->descriptor;
	struct device *dev = dd->dev;
	uint32_t *key_iv = kzalloc(DMA_KEY_IV_BUF_SIZE, GFP_ATOMIC);
	uint32_t *piv = key_iv + 8;
	uint32_t len = keylen;
	dma_addr_t dma_addr_key;
	uint32_t i = 0;

	if (!key_iv) {
		dev_err(dev, "error allocating key_iv buffer\n");
		return -EINVAL;
	}
	memcpy(key_iv, key, keylen);
	if (iv) {
		memcpy(piv, iv, 8);
	}

	len = DMA_KEY_IV_BUF_SIZE; /* full key storage */

	dma_addr_key = dma_map_single(dd->dev, key_iv,
			DMA_KEY_IV_BUF_SIZE, DMA_TO_DEVICE);

	if (dma_mapping_error(dd->dev, dma_addr_key)) {
		dev_err(dev, "error mapping dma_addr_key\n");
		kfree(key_iv);
		return -EINVAL;
	}

	while (len > 0) {
		dsc[i].src_addr = (uint32_t)dma_addr_key + i * 16;
		dsc[i].tgt_addr = i * 16;
		dsc[i].dsc_cfg.d32 = 0;
		dsc[i].dsc_cfg.b.length = len > 16 ? 16 : len;
		dsc[i].dsc_cfg.b.mode = MODE_KEY;
		dsc[i].dsc_cfg.b.eoc = 0;
		dsc[i].dsc_cfg.b.owner = 1;
		i++;
		len -= 16;
	}
	dsc[i - 1].dsc_cfg.b.eoc = 1;

	dma_sync_single_for_device(dd->dev, dd->dma_descript_tab,
			PAGE_SIZE, DMA_TO_DEVICE);
	aml_write_crypto_reg(dd->thread,
			(uintptr_t) dd->dma_descript_tab | 2);
	aml_dma_debug(dsc, i, __func__, dd->thread, dd->status);
	while (aml_read_crypto_reg(dd->status) == 0)
		;
	aml_write_crypto_reg(dd->status, 0xf);
	dma_unmap_single(dd->dev, dma_addr_key,
			DMA_KEY_IV_BUF_SIZE, DMA_TO_DEVICE);

	kfree(key_iv);
	return 0;
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

#if SUPPORT_FAST_DMA
static size_t aml_tdes_sg_dma(struct aml_tdes_dev *dd, struct dma_dsc *dsc,
		uint32_t *nents, size_t total)
{
	struct device *dev = dd->dev;
	size_t count = 0;
	size_t process = 0;
	size_t count_total = 0;
	size_t count_sg = 0;
	uint32_t i = 0;
	int err = 0;
	struct scatterlist *in_sg = dd->in_sg;
	struct scatterlist *out_sg = dd->out_sg;
	dma_addr_t addr_in, addr_out;

	while (total && in_sg && out_sg && (in_sg->length == out_sg->length)
			&& IS_ALIGNED(in_sg->length, DES_BLOCK_SIZE)
			&& *nents < MAX_NUM_TABLES) {
		process = min_t(unsigned int, total, in_sg->length);
		count += process;
		*nents += 1;
		if (process != in_sg->length)
			dd->out_offset = dd->in_offset = in_sg->length;
		total -= process;
		in_sg = sg_next(in_sg);
		out_sg = sg_next(out_sg);
	}
	if (dd->in_sg != dd->out_sg) {
		err = dma_map_sg(dd->dev, dd->in_sg, *nents, DMA_TO_DEVICE);
		if (!err) {
			dev_err(dev, "dma_map_sg() error\n");
			return 0;
		}

		err = dma_map_sg(dd->dev, dd->out_sg, *nents,
				DMA_FROM_DEVICE);
		if (!err) {
			dev_err(dev, "dma_map_sg() error\n");
			dma_unmap_sg(dd->dev, dd->in_sg, *nents,
					DMA_TO_DEVICE);
			return 0;
		}
	} else {
		err = dma_map_sg(dd->dev, dd->in_sg, *nents,
				DMA_BIDIRECTIONAL);
		if (!err) {
			dev_err(dev, "dma_map_sg() error\n");
			return 0;
		}
		dma_sync_sg_for_device(dd->dev, dd->in_sg,
				*nents, DMA_TO_DEVICE);
	}

	in_sg = dd->in_sg;
	out_sg = dd->out_sg;
	count_total = count;
	for (i = 0; i < *nents; i++) {
		count_sg = count_total > sg_dma_len(in_sg) ?
			sg_dma_len(in_sg) : count_total;
		addr_in = sg_dma_address(in_sg);
		addr_out = sg_dma_address(out_sg);
		dsc[i].src_addr = (uintptr_t)addr_in;
		dsc[i].tgt_addr = (uintptr_t)addr_out;
		dsc[i].dsc_cfg.d32 = 0;
		dsc[i].dsc_cfg.b.length = count_sg;
		in_sg = sg_next(in_sg);
		out_sg = sg_next(out_sg);
		count_total -= count_sg;
	}
	return count;
}
#endif
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
	struct ablkcipher_request *req = dd->req;
	unsigned long flags;

	spin_lock_irqsave(&dd->dma->dma_lock, flags);
	dd->flags &= ~TDES_FLAGS_BUSY;
	dd->dma->dma_busy &= ~DMA_FLAG_MAY_OCCUPY;
	spin_unlock_irqrestore(&dd->dma->dma_lock, flags);
	req->base.complete(&req->base, err);
}

static int aml_tdes_crypt_dma(struct aml_tdes_dev *dd, struct dma_dsc *dsc,
		uint32_t nents)
{
	uint32_t op_mode = OP_MODE_ECB;
	uint32_t i = 0;
	unsigned long flags;


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

	dma_sync_single_for_device(dd->dev, dd->dma_descript_tab,
			PAGE_SIZE, DMA_TO_DEVICE);

	aml_dma_debug(dsc, nents, __func__, dd->thread, dd->status);

	/* Start DMA transfer */
	spin_lock_irqsave(&dd->dma->dma_lock, flags);
	dd->dma->dma_busy |= DMA_FLAG_TDES_IN_USE;
	spin_unlock_irqrestore(&dd->dma->dma_lock, flags);

	dd->flags |= TDES_FLAGS_DMA;
	aml_write_crypto_reg(dd->thread,
			(uintptr_t) dd->dma_descript_tab | 2);
	return -EINPROGRESS;
}

static int aml_tdes_crypt_dma_start(struct aml_tdes_dev *dd)
{
	int err = 0, fast = 0;
	int in, out;
	size_t count;
	dma_addr_t addr_in, addr_out;
	struct dma_dsc *dsc = dd->descriptor;
	uint32_t nents;

	/* fast dma */
	if ((!dd->in_offset) && (!dd->out_offset)) {
		/* check for alignment */
		in = IS_ALIGNED(dd->in_sg->length, dd->ctx->block_size);
		out = IS_ALIGNED(dd->out_sg->length, dd->ctx->block_size);
		fast = in && out;

		if (dd->in_sg->length != dd->out_sg->length
				|| dd->total < dd->ctx->block_size)
			fast = 0;
		dd->fast_nents = 0;
	}

#if SUPPORT_FAST_DMA
	if (fast)  {
		count = aml_tdes_sg_dma(dd, dsc, &dd->fast_nents, dd->total);
		dd->flags |= TDES_FLAGS_FAST;
		nents = dd->fast_nents;
	} else
#endif
	{
		/* slow dma */
		/* use cache buffers */
		count = aml_tdes_sg_copy(&dd->in_sg, &dd->in_offset,
				dd->buf_in, dd->buflen, dd->total, 0);
		addr_in = dd->dma_addr_in;
		addr_out = dd->dma_addr_out;
		dd->dma_size = count;
		dma_sync_single_for_device(dd->dev, addr_in, dd->dma_size,
				DMA_TO_DEVICE);
		dsc->src_addr = (uint32_t)addr_in;
		dsc->tgt_addr = (uint32_t)addr_out;
		dsc->dsc_cfg.d32 = 0;
		dsc->dsc_cfg.b.length = count;
		nents = 1;
		dd->flags &= ~TDES_FLAGS_FAST;
	}

	dd->total -= count;
	err = aml_tdes_crypt_dma(dd, dsc, nents);

	return err;
}

static int aml_tdes_write_ctrl(struct aml_tdes_dev *dd)
{
	int err = 0;

	err = aml_tdes_hw_init(dd);

	if (err)
		return err;

	if (dd->flags & TDES_FLAGS_CBC)
		err = set_tdes_key_iv(dd, dd->ctx->key, dd->ctx->keylen,
				dd->req->info);
	else
		err = set_tdes_key_iv(dd, dd->ctx->key, dd->ctx->keylen,
				NULL);

	return err;
}

static int aml_tdes_handle_queue(struct aml_tdes_dev *dd,
		struct ablkcipher_request *req)
{
	struct crypto_async_request *async_req, *backlog;
	struct aml_tdes_ctx *ctx;
	struct aml_tdes_reqctx *rctx;
	unsigned long flags;
	int err, ret = 0;

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

	if (!async_req)
		return ret;

	if (backlog)
		backlog->complete(backlog, -EINPROGRESS);

	req = ablkcipher_request_cast(async_req);

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
	dd->ctx = ctx;
	ctx->dd = dd;

	err = aml_tdes_write_ctrl(dd);
	if (!err)
		err = aml_tdes_crypt_dma_start(dd);

	if (err != -EINPROGRESS) {
		/* tdes_task will not finish it, so do it here */
		aml_tdes_finish_req(dd, err);
		tasklet_schedule(&dd->queue_task);
	}

	return ret;
}

static int aml_tdes_crypt_dma_stop(struct aml_tdes_dev *dd)
{
	struct device *dev = dd->dev;
	int err = -EINVAL;
	size_t count;

	if (dd->flags & TDES_FLAGS_DMA) {
		err = 0;
		dma_sync_single_for_cpu(dd->dev, dd->dma_descript_tab,
				PAGE_SIZE, DMA_FROM_DEVICE);
		if  (dd->flags & TDES_FLAGS_FAST) {
			if (dd->in_sg != dd->out_sg) {
				dma_unmap_sg(dd->dev, dd->out_sg,
					dd->fast_nents, DMA_FROM_DEVICE);
				dma_unmap_sg(dd->dev, dd->in_sg,
					dd->fast_nents, DMA_TO_DEVICE);
			} else {
				dma_sync_sg_for_cpu(dd->dev, dd->in_sg,
				dd->fast_nents, DMA_FROM_DEVICE);
				dma_unmap_sg(dd->dev, dd->in_sg,
					dd->fast_nents, DMA_BIDIRECTIONAL);
			}
		} else {
			dma_sync_single_for_cpu(dd->dev, dd->dma_addr_out,
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
	dd->descriptor = (void *)__get_free_pages(GFP_KERNEL, 0);
	dd->buflen = PAGE_SIZE;
	dd->buflen &= ~(DES_BLOCK_SIZE - 1);

	if (!dd->buf_in || !dd->buf_out || !dd->descriptor) {
		dev_err(dev, "unable to alloc pages.\n");
		goto err_alloc;
	}

	/* MAP here */
	dd->dma_addr_in = dma_map_single(dd->dev, dd->buf_in,
			dd->buflen, DMA_TO_DEVICE);
	if (dma_mapping_error(dd->dev, dd->dma_addr_in)) {
		dev_err(dev, "dma %zd bytes error\n", dd->buflen);
		err = -EINVAL;
		goto err_map_in;
	}

	dd->dma_addr_out = dma_map_single(dd->dev, dd->buf_out,
			dd->buflen, DMA_FROM_DEVICE);
	if (dma_mapping_error(dd->dev, dd->dma_addr_out)) {
		dev_err(dev, "dma %zd bytes error\n", dd->buflen);
		err = -EINVAL;
		goto err_map_out;
	}

	dd->dma_descript_tab = dma_map_single(dd->dev, dd->descriptor,
			PAGE_SIZE, DMA_TO_DEVICE);

	if (dma_mapping_error(dd->dev, dd->dma_descript_tab)) {
		dev_err(dev, "dma descriptor error\n");
		err = -EINVAL;
		goto err_map_descriptor;
	}


	return 0;

err_map_descriptor:
	dma_unmap_single(dd->dev, dd->dma_descript_tab, PAGE_SIZE,
			DMA_TO_DEVICE);

err_map_out:
	dma_unmap_single(dd->dev, dd->dma_addr_in, dd->buflen,
			DMA_TO_DEVICE);
err_map_in:
	free_page((uintptr_t)dd->buf_out);
	free_page((uintptr_t)dd->buf_in);
	free_page((uintptr_t)dd->descriptor);
err_alloc:
	if (err)
		dev_err(dev, "error: %d\n", err);
	return err;
}

static void aml_tdes_buff_cleanup(struct aml_tdes_dev *dd)
{
	dma_unmap_single(dd->dev, dd->dma_addr_out, dd->buflen,
			DMA_FROM_DEVICE);
	dma_unmap_single(dd->dev, dd->dma_addr_in, dd->buflen,
			DMA_TO_DEVICE);
	dma_unmap_single(dd->dev, dd->dma_descript_tab, PAGE_SIZE,
			DMA_TO_DEVICE);
	free_page((unsigned long)dd->buf_out);
	free_page((unsigned long)dd->buf_in);
	free_page((uintptr_t)dd->descriptor);
}

static int aml_tdes_crypt(struct ablkcipher_request *req, unsigned long mode)
{
	struct aml_tdes_ctx *ctx = crypto_ablkcipher_ctx(
			crypto_ablkcipher_reqtfm(req));
	struct aml_tdes_reqctx *rctx = ablkcipher_request_ctx(req);
	struct aml_tdes_dev *dd;

	if (!IS_ALIGNED(req->nbytes, DES_BLOCK_SIZE)) {
		pr_err("request size is not exact amount of TDES blocks\n");
		return -EINVAL;
	}
	ctx->block_size = DES_BLOCK_SIZE;

	if (ctx->fallback && ctx->same_key) {
		char *__subreq_desc = kzalloc(sizeof(struct skcipher_request) +
				crypto_skcipher_reqsize(ctx->fallback),
				GFP_ATOMIC);
		struct skcipher_request *subreq = (void *)__subreq_desc;
		int ret = 0;

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

	return aml_tdes_handle_queue(dd, req);
}

static int aml_des_setkey(struct crypto_ablkcipher *tfm, const u8 *key,
		u32 keylen)
{
	u32 tmp[DES_EXPKEY_WORDS];
	int err;
	struct crypto_tfm *ctfm = crypto_ablkcipher_tfm(tfm);
	struct aml_tdes_ctx *ctx = crypto_ablkcipher_ctx(tfm);

	if (keylen != DES_KEY_SIZE) {
		crypto_ablkcipher_set_flags(tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);
		return -EINVAL;
	}

	err = des_ekey(tmp, key);
	if (err == 0 && (ctfm->crt_flags & CRYPTO_TFM_REQ_WEAK_KEY)) {
		ctfm->crt_flags |= CRYPTO_TFM_RES_WEAK_KEY;
		return -EINVAL;
	}

	memcpy(ctx->key, key, keylen);
	ctx->keylen = keylen;

	return 0;
}

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

	return 0;
}

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

static struct crypto_alg des_tdes_algs[] = {
	{
		.cra_name        = "ecb(des)",
		.cra_driver_name = "ecb-des-aml",
		.cra_priority  =  200,
		.cra_flags     =  CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize =  DES_BLOCK_SIZE,
		.cra_ctxsize   =  sizeof(struct aml_tdes_ctx),
		.cra_alignmask =  0,
		.cra_type      =  &crypto_ablkcipher_type,
		.cra_module    =  THIS_MODULE,
		.cra_init      =  aml_tdes_cra_init,
		.cra_exit      =  aml_tdes_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    DES_KEY_SIZE,
			.max_keysize	=    DES_KEY_SIZE,
			.setkey		=    aml_des_setkey,
			.encrypt	=    aml_tdes_ecb_encrypt,
			.decrypt	=    aml_tdes_ecb_decrypt,
		}
	},
	{
		.cra_name        =  "cbc(des)",
		.cra_driver_name =  "cbc-des-aml",
		.cra_priority  =  200,
		.cra_flags     =  CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize =  DES_BLOCK_SIZE,
		.cra_ctxsize   =  sizeof(struct aml_tdes_ctx),
		.cra_alignmask =  0,
		.cra_type      =  &crypto_ablkcipher_type,
		.cra_module    =  THIS_MODULE,
		.cra_init      =  aml_tdes_cra_init,
		.cra_exit      =  aml_tdes_cra_exit,
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
		.cra_name        = "ecb(des3_ede)",
		.cra_driver_name = "ecb-tdes-aml",
		.cra_priority   = 200,
		.cra_flags      = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize  = DES_BLOCK_SIZE,
		.cra_ctxsize    = sizeof(struct aml_tdes_ctx),
		.cra_alignmask  = 0,
		.cra_type       = &crypto_ablkcipher_type,
		.cra_module     = THIS_MODULE,
		.cra_init       = aml_tdes_cra_init,
		.cra_exit       = aml_tdes_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize	=    2 * DES_KEY_SIZE,
			.max_keysize	=    3 * DES_KEY_SIZE,
			.setkey		=    aml_tdes_setkey,
			.encrypt	=    aml_tdes_ecb_encrypt,
			.decrypt	=    aml_tdes_ecb_decrypt,
		}
	},
	{
		.cra_name        = "cbc(des3_ede)",
		.cra_driver_name = "cbc-tdes-aml",
		.cra_priority  = 200,
		.cra_flags     = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize = DES_BLOCK_SIZE,
		.cra_ctxsize   = sizeof(struct aml_tdes_ctx),
		.cra_alignmask = 0,
		.cra_type      = &crypto_ablkcipher_type,
		.cra_module    = THIS_MODULE,
		.cra_init      = aml_tdes_cra_init,
		.cra_exit      = aml_tdes_cra_exit,
		.cra_u.ablkcipher =       {
			.min_keysize = 2 * DES_KEY_SIZE,
			.max_keysize = 3 * DES_KEY_SIZE,
			.ivsize	     = DES_BLOCK_SIZE,
			.setkey	     = aml_tdes_setkey,
			.encrypt     = aml_tdes_cbc_encrypt,
			.decrypt     = aml_tdes_cbc_decrypt,
		}
	}
};

static int aml_tdes_lite_cra_init(struct crypto_tfm *tfm)
{
	struct aml_tdes_ctx *ctx = crypto_tfm_ctx(tfm);
	const char *alg_name = crypto_tfm_alg_name(tfm);
	const u32 flags = CRYPTO_ALG_ASYNC | CRYPTO_ALG_NEED_FALLBACK;

	tfm->crt_ablkcipher.reqsize = sizeof(struct aml_tdes_reqctx);

	/* Allocate a fallback and abort if it failed. */
	ctx->fallback = crypto_alloc_skcipher(alg_name, 0,
					    flags);
	if (IS_ERR(ctx->fallback)) {
		pr_err("aml-tdes: fallback '%s' could not be loaded.\n",
				alg_name);
		return PTR_ERR(ctx->fallback);
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
		.cra_name        = "ecb(des3_ede)",
		.cra_driver_name = "ecb-tdes-lite-aml",
		.cra_priority   = 200,
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
		.cra_name        = "cbc(des3_ede)",
		.cra_driver_name = "cbc-tdes-lite-aml",
		.cra_priority  = 200,
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
	}
};

static void aml_tdes_queue_task(unsigned long data)
{
	struct aml_tdes_dev *dd = (struct aml_tdes_dev *)data;

	aml_tdes_handle_queue(dd, NULL);
}

static void aml_tdes_done_task(unsigned long data)
{
	struct aml_tdes_dev *dd = (struct aml_tdes_dev *) data;
	struct device *dev = dd->dev;
	int err;

	err = aml_tdes_crypt_dma_stop(dd);

	aml_dma_debug(dd->descriptor, dd->fast_nents ?
			dd->fast_nents : 1, __func__, dd->thread, dd->status);
	err = dd->err ? : err;

	if (dd->total && !err) {
		if (dd->flags & TDES_FLAGS_FAST) {
			uint32_t i = 0;

			for (i = 0; i < dd->fast_nents; i++) {
				dd->in_sg = sg_next(dd->in_sg);
				dd->out_sg = sg_next(dd->out_sg);
				if (!dd->in_sg || !dd->out_sg) {
					dev_err(dev, "aml-tdes: sg invalid\n");
					err = -EINVAL;
					break;
				}
			}
		}

		if (!err)
			err = aml_tdes_crypt_dma_start(dd);
		if (err == -EINPROGRESS)
			return; /* DMA started. Not fininishing. */
	}

	aml_tdes_finish_req(dd, err);
	aml_tdes_handle_queue(dd, NULL);
}

static irqreturn_t aml_tdes_irq(int irq, void *dev_id)
{
	struct aml_tdes_dev *tdes_dd = dev_id;
	struct device *dev = tdes_dd->dev;
	uint8_t status = aml_read_crypto_reg(tdes_dd->status);

	if (status) {
		if (status == 0x1)
			dev_err(dev, "irq overwrite\n");
		if (tdes_dd->dma->dma_busy == DMA_FLAG_MAY_OCCUPY)
			return IRQ_HANDLED;
		if ((tdes_dd->dma->dma_busy & DMA_FLAG_TDES_IN_USE) &&
				(tdes_dd->flags & TDES_FLAGS_DMA)) {
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

static void aml_tdes_unregister_algs(struct aml_tdes_dev *dd,
		const struct aml_tdes_info *tdes_info)
{
	int i;

	for (i = 0; i < tdes_info->num_algs; i++)
		crypto_unregister_alg(&(tdes_info->algs[i]));
}

static int aml_tdes_register_algs(struct aml_tdes_dev *dd,
		const struct aml_tdes_info *tdes_info)
{
	int err, i, j;

	for (i = 0; i < tdes_info->num_algs; i++) {
		err = crypto_register_alg(&(tdes_info->algs[i]));
		if (err)
			goto err_tdes_algs;
	}

	return 0;

err_tdes_algs:
	for (j = 0; j < i; j++)
		crypto_unregister_alg(&(tdes_info->algs[j]));

	return err;
}

struct aml_tdes_info aml_des_tdes = {
	.algs = des_tdes_algs,
	.num_algs = ARRAY_SIZE(des_tdes_algs),
};

struct aml_tdes_info aml_tdes_lite = {
	.algs = tdes_lite_algs,
	.num_algs = ARRAY_SIZE(tdes_lite_algs),
};

#ifdef CONFIG_OF
static const struct of_device_id aml_tdes_dt_match[] = {
	{	.compatible = "amlogic,des_dma,tdes_dma",
		.data = &aml_des_tdes,
	},
	{	.compatible = "amlogic,tdes_dma",
		.data = &aml_tdes_lite,
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
	const struct of_device_id *match;
	const struct aml_tdes_info *tdes_info = NULL;

	tdes_dd = devm_kzalloc(dev, sizeof(struct aml_tdes_dev), GFP_KERNEL);
	if (tdes_dd == NULL) {
		err = -ENOMEM;
		goto tdes_dd_err;
	}

	match = of_match_device(aml_tdes_dt_match, &pdev->dev);
	if (!match) {
		dev_err(dev, "%s: cannot find match dt\n", __func__);
		err = -EINVAL;
		goto tdes_dd_err;
	}

	tdes_info = match->data;
	tdes_dd->dev = dev;
	tdes_dd->dma = dev_get_drvdata(dev->parent);
	tdes_dd->thread = tdes_dd->dma->thread;
	tdes_dd->status = tdes_dd->dma->status;
	tdes_dd->irq = tdes_dd->dma->irq;

	platform_set_drvdata(pdev, tdes_dd);

	INIT_LIST_HEAD(&tdes_dd->list);

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

	return 0;

err_algs:
	spin_lock(&aml_tdes.lock);
	list_del(&tdes_dd->list);
	spin_unlock(&aml_tdes.lock);
	aml_tdes_buff_cleanup(tdes_dd);
err_tdes_buff:
tdes_irq_err:
	tasklet_kill(&tdes_dd->done_task);
	tasklet_kill(&tdes_dd->queue_task);
tdes_dd_err:
	dev_err(dev, "initialization failed.\n");

	return err;
}

static int aml_tdes_remove(struct platform_device *pdev)
{
	static struct aml_tdes_dev *tdes_dd;
	struct device *dev = &pdev->dev;
	const struct of_device_id *match;
	const struct aml_tdes_info *tdes_info = NULL;

	tdes_dd = platform_get_drvdata(pdev);
	if (!tdes_dd)
		return -ENODEV;

	match = of_match_device(aml_tdes_dt_match, &pdev->dev);
	if (!match) {
		dev_err(dev, "%s: cannot find match dt\n", __func__);
		return -EINVAL;
	}

	tdes_info = match->data;
	spin_lock(&aml_tdes.lock);
	list_del(&tdes_dd->list);
	spin_unlock(&aml_tdes.lock);

	aml_tdes_unregister_algs(tdes_dd, tdes_info);

	tasklet_kill(&tdes_dd->done_task);
	tasklet_kill(&tdes_dd->queue_task);

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

module_platform_driver(aml_tdes_driver);

MODULE_DESCRIPTION("Aml TDES hw acceleration support.");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("matthew.shyu <matthew.shyu@amlogic.com>");
