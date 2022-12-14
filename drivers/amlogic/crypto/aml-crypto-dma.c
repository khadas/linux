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
#include <crypto/internal/hash.h>
#include "aml-crypto-dma.h"

#define DEBUG				(0)

u32 swap_ulong32(u32 val)
{
	u32 res = 0;

	res = ((val & 0xff) << 24) + (((val >> 8) & 0xff) << 16) +
	       (((val >> 16) & 0xff) << 8) + ((val >> 24) & 0xff);
	return res;
}
EXPORT_SYMBOL_GPL(swap_ulong32);

void aml_write_crypto_reg(u32 addr, u32 data)
{
	if (cryptoreg)
		writel(data, cryptoreg + (addr << 2));
	else
		pr_err("crypto reg mapping is not initialized\n");
}
EXPORT_SYMBOL_GPL(aml_write_crypto_reg);

u32 aml_read_crypto_reg(u32 addr)
{
	if (!cryptoreg) {
		pr_err("crypto reg mapping is not initialized\n");
		return 0;
	}
	return readl(cryptoreg + (addr << 2));
}
EXPORT_SYMBOL_GPL(aml_read_crypto_reg);

void aml_dma_debug(struct dma_dsc *dsc, u32 nents, const char *msg,
		   u32 thread, u32 status)
{
	u32 i = 0;

	dbgp(0, "begin %s\n", msg);
	dbgp(0, "reg(%u) = 0x%8x\n", thread,
	     aml_read_crypto_reg(thread));
	dbgp(0, "reg(%u) = 0x%8x\n", status,
	     aml_read_crypto_reg(status));
	for (i = 0; i < nents; i++) {
		dbgp(0, "desc (%4x) (len) = 0x%8x\n", i,
		     dsc[i].dsc_cfg.b.length);
		dbgp(0, "desc (%4x) (irq) = 0x%8x\n", i,
		     dsc[i].dsc_cfg.b.irq);
		dbgp(0, "desc (%4x) (eoc) = 0x%8x\n", i,
		     dsc[i].dsc_cfg.b.eoc);
		dbgp(0, "desc (%4x) (lop) = 0x%8x\n", i,
		     dsc[i].dsc_cfg.b.loop);
		dbgp(0, "desc (%4x) (mod) = 0x%8x\n", i,
		     dsc[i].dsc_cfg.b.mode);
		dbgp(0, "desc (%4x) (beg) = 0x%8x\n", i,
		     dsc[i].dsc_cfg.b.begin);
		dbgp(0, "desc (%4x) (end) = 0x%8x\n", i,
		     dsc[i].dsc_cfg.b.end);
		dbgp(0, "desc (%4x) (opm) = 0x%8x\n", i,
		     dsc[i].dsc_cfg.b.op_mode);
		dbgp(0, "desc (%4x) (enc) = 0x%8x\n", i,
		     dsc[i].dsc_cfg.b.enc_sha_only);
		dbgp(0, "desc (%4x) (blk) = 0x%8x\n", i,
		     dsc[i].dsc_cfg.b.block);
		dbgp(0, "desc (%4x) (lnk_err) = 0x%8x\n", i,
		     dsc[i].dsc_cfg.b.link_error);
		dbgp(0, "desc (%4x) (own) = 0x%8x\n", i,
		     dsc[i].dsc_cfg.b.owner);
		dbgp(0, "desc (%4x) (src) = 0x%8x\n", i,
		     dsc[i].src_addr);
		dbgp(0, "desc (%4x) (tgt) = 0x%8x\n", i,
		     dsc[i].tgt_addr);
	}
	dbgp(0, "end %s\n", msg);
}
EXPORT_SYMBOL_GPL(aml_dma_debug);

void aml_dma_link_debug(struct dma_sg_dsc *dsc, dma_addr_t dma_dsc,
			u32 nents, const char *msg)
{
	u32 i = 0;

	dbgp(0, "link begin %s\n", msg);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	dbgp(0, "link dma_addr 0x%llx\n", dma_dsc);
#else
	dbgp(0, "link dma_addr 0x%x\n", dma_dsc);
#endif
	for (i = 0; i < nents; i++) {
		dbgp(0, "sg desc (%4x) (vld) = 0x%8x\n", i,
		     dsc[i].dsc_cfg.b.valid);
		dbgp(0, "sg desc (%4x) (eoc) = 0x%8x\n", i,
		     dsc[i].dsc_cfg.b.eoc);
		dbgp(0, "sg desc (%4x) (int) = 0x%8x\n", i,
		     dsc[i].dsc_cfg.b.intr);
		dbgp(0, "sg desc (%4x) (act) = 0x%8x\n", i,
		     dsc[i].dsc_cfg.b.act);
		dbgp(0, "sg desc (%4x) (len) = 0x%8x\n", i,
		     dsc[i].dsc_cfg.b.length);
		dbgp(0, "sg desc (%4x) (adr) = 0x%8x\n", i,
		     dsc[i].addr);
	}
	dbgp(0, "link end %s\n", msg);
}
EXPORT_SYMBOL_GPL(aml_dma_link_debug);

u8 aml_dma_do_hw_crypto(struct aml_dma_dev *dd,
			  struct dma_dsc *dsc,
			  u32 dsc_len,
			  dma_addr_t dsc_addr,
			  u8 polling, u8 dma_flags)
{
	u8 status = 0;

	spin_lock_irqsave(&dd->dma_lock, dd->irq_flags);
	dd->dma_busy |= dma_flags;
	aml_write_crypto_reg(dd->thread, (uintptr_t)dsc_addr | 2);
	if (polling) {
		while (aml_read_crypto_reg(dd->status) == 0)
			;
		status = aml_read_crypto_reg(dd->status);
		WARN_ON(!(dsc[dsc_len - 1].dsc_cfg.b.eoc == 1));
		if (dsc[dsc_len - 1].dsc_cfg.b.eoc == 1) {
			while (dsc[dsc_len - 1].dsc_cfg.b.owner)
				cpu_relax();
		}
		aml_write_crypto_reg(dd->status, 0xf);
		dd->dma_busy &= ~dma_flags;
	}
	spin_unlock_irqrestore(&dd->dma_lock, dd->irq_flags);

	return status;
}
EXPORT_SYMBOL_GPL(aml_dma_do_hw_crypto);

void aml_dma_finish_hw_crypto(struct aml_dma_dev *dd, u8 dma_flags)
{
	spin_lock_irqsave(&dd->dma_lock, dd->irq_flags);
	aml_write_crypto_reg(dd->status, 0xf);
	dd->dma_busy &= ~dma_flags;
	spin_unlock_irqrestore(&dd->dma_lock, dd->irq_flags);
}
EXPORT_SYMBOL_GPL(aml_dma_finish_hw_crypto);

int aml_dma_crypto_enqueue_req(struct aml_dma_dev *dd,
			       struct crypto_async_request *req)
{
	s32 ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&dd->queue_lock, flags);
	ret = crypto_enqueue_request(&dd->queue, req);
	spin_unlock_irqrestore(&dd->queue_lock, flags);
	wake_up_process(dd->kthread);

	return ret;
}
EXPORT_SYMBOL_GPL(aml_dma_crypto_enqueue_req);

#if DEBUG
void dump_buf(const s8 *name, u8 *buf, const s32 length)
{
	s32 i = 0;
	s32 j = length;

	pr_err("\nstart dump(%s, %d)================\n", name, length);
	while (j >= 8) {
		pr_err("%#02x ~ %#02x:\n", i, i + 7);
		pr_err("%#02x, %#02x, %#02x, %#02x, %#02x, %#02x, %#02x, %#02x\n",
		       buf[i], buf[i + 1], buf[i + 2], buf[i + 3],
		       buf[i + 4], buf[i + 5], buf[i + 6], buf[i + 7]);
		i += 8;
		j -= 8;
	};

	while (j > 0) {
		pr_err("%#02x ", buf[i]);
		j--;
		i++;
	};
	pr_err("\nend dump(%s, %d)================\n", name, length);
}
#endif
