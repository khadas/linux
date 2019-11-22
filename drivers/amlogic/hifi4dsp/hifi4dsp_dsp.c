/*
 * drivers/amlogic/hifi4dsp/hifi4dsp_dsp.c
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
#define DEBUG

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/firmware.h>
#include <linux/amlogic/major.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>

#include "hifi4dsp_priv.h"
#include "hifi4dsp_firmware.h"
#include "hifi4dsp_dsp.h"

//static struct hifi4dsp_dsp *dsp_g;

/* Internal generic low-level hifi4dsp share memory write/read functions*/
void hifi4dsp_smem_write(void __iomem *addr, u32 offset, u32 value)
{
	writel(value, addr + offset);
}
EXPORT_SYMBOL(hifi4dsp_smem_write);

u32 hifi4dsp_smem_read(void __iomem *addr, u32 offset)
{
	return readl(addr + offset);
}
EXPORT_SYMBOL(hifi4dsp_smem_read);

void hifi4dsp_smem_write64(void __iomem *addr, u32 offset, u64 value)
{
	memcpy_toio(addr + offset, &value, sizeof(value));
}
EXPORT_SYMBOL(hifi4dsp_smem_write64);

u64 hifi4dsp_smem_read64(void __iomem *addr, u32 offset)
{
	u64 val;

	memcpy_fromio(&val, addr + offset, sizeof(val));
	return val;
}
EXPORT_SYMBOL(hifi4dsp_smem_read64);

static inline void _hifi4dsp_memcpy_toio_32(u32 __iomem *dest,
	u32 *src, size_t bytes)
{
	int i, words = bytes >> 2;

	for (i = 0; i < words; i++)
		writel(src[i], dest + i);
}

static inline void _hifi4dsp_memcpy_fromio_32(u32 *dest,
	const __iomem u32 *src, size_t bytes)
{
	int i, words = bytes >> 2;

	for (i = 0; i < words; i++)
		dest[i] = readl(src + i);
}

void hifi4dsp_memcpy_toio_32(struct hifi4dsp_dsp *dsp,
	void __iomem *dest, void *src, size_t bytes)
{
	_hifi4dsp_memcpy_toio_32(dest, src, bytes);
}
EXPORT_SYMBOL(hifi4dsp_memcpy_toio_32);

void hifi4dsp_memcpy_fromio_32(struct hifi4dsp_dsp *dsp, void *dest,
	void __iomem *src, size_t bytes)
{
	_hifi4dsp_memcpy_fromio_32(dest, src, bytes);
}
EXPORT_SYMBOL(hifi4dsp_memcpy_fromio_32);

/* Public API */
void hifi4dsp_dsp_smem_write(struct hifi4dsp_dsp *dsp, u32 offset, u32 value)
{
	unsigned long flags;

	spin_lock_irqsave(&dsp->spinlock, flags);
	dsp->ops->write(dsp->addr.smem, offset, value);
	spin_unlock_irqrestore(&dsp->spinlock, flags);
}
EXPORT_SYMBOL(hifi4dsp_dsp_smem_write);

u32 hifi4dsp_dsp_smem_read(struct hifi4dsp_dsp *dsp, u32 offset)
{
	unsigned long flags;
	u32 val;

	spin_lock_irqsave(&dsp->spinlock, flags);
	val = dsp->ops->read(dsp->addr.smem, offset);
	spin_unlock_irqrestore(&dsp->spinlock, flags);

	return val;
}
EXPORT_SYMBOL(hifi4dsp_dsp_smem_read);

void hifi4dsp_dsp_smem_write64(struct hifi4dsp_dsp *dsp, u32 offset, u64 value)
{
	unsigned long flags;

	spin_lock_irqsave(&dsp->spinlock, flags);
	dsp->ops->write64(dsp->addr.smem, offset, value);
	spin_unlock_irqrestore(&dsp->spinlock, flags);
}
EXPORT_SYMBOL(hifi4dsp_dsp_smem_write64);

u64 hifi4dsp_dsp_smem_read64(struct hifi4dsp_dsp *dsp, u32 offset)
{
	unsigned long flags;
	u64 val;

	spin_lock_irqsave(&dsp->spinlock, flags);
	val = dsp->ops->read64(dsp->addr.smem, offset);
	spin_unlock_irqrestore(&dsp->spinlock, flags);

	return val;
}
EXPORT_SYMBOL(hifi4dsp_dsp_smem_read64);

void hifi4dsp_dsp_smem_write_unlocked(struct hifi4dsp_dsp *dsp,
			u32 offset, u32 value)
{
	dsp->ops->write(dsp->addr.smem, offset, value);
}
EXPORT_SYMBOL(hifi4dsp_dsp_smem_write_unlocked);

u32 hifi4dsp_dsp_smem_read_unlocked(struct hifi4dsp_dsp *dsp,
		u32 offset)
{
	return dsp->ops->read(dsp->addr.smem, offset);
}
EXPORT_SYMBOL(hifi4dsp_dsp_smem_read_unlocked);

void hifi4dsp_dsp_smem_write64_unlocked(struct hifi4dsp_dsp *dsp,
		u32 offset, u64 value)
{
	dsp->ops->write64(dsp->addr.smem, offset, value);
}
EXPORT_SYMBOL(hifi4dsp_dsp_smem_write64_unlocked);

u64 hifi4dsp_dsp_smem_read64_unlocked(struct hifi4dsp_dsp *dsp,
		u32 offset)
{
	return dsp->ops->read64(dsp->addr.smem, offset);
}
EXPORT_SYMBOL(hifi4dsp_dsp_smem_read64_unlocked);

int hifi4dsp_dsp_smem_update_bits_unlocked(struct hifi4dsp_dsp *dsp,
	u32 offset,	u32 mask, u32 value)
{
	bool change;
	unsigned int old, new;
	u32 ret;

	ret = hifi4dsp_dsp_smem_read_unlocked(dsp, offset);

	old = ret;
	new = (old & (~mask)) | (value & mask);

	change = (old != new);
	if (change)
		hifi4dsp_dsp_smem_write_unlocked(dsp, offset, new);

	return change;
}
EXPORT_SYMBOL(hifi4dsp_dsp_smem_update_bits_unlocked);

void hifi4dsp_dsp_smem_update_bits_forced_unlocked(struct hifi4dsp_dsp *dsp,
		u32 offset,	u32 mask, u32 value)
{
	unsigned int old, new;
	u32 ret;

	ret = hifi4dsp_dsp_smem_read_unlocked(dsp, offset);

	old = ret;
	new = (old & (~mask)) | (value & mask);

	hifi4dsp_dsp_smem_write_unlocked(dsp, offset, new);
}
EXPORT_SYMBOL(hifi4dsp_dsp_smem_update_bits_forced_unlocked);

int hifi4dsp_dsp_smem_update_bits64_unlocked(struct hifi4dsp_dsp *dsp,
		u32 offset,	u64 mask, u64 value)
{
	bool change;
	u64 old, new;

	old = hifi4dsp_dsp_smem_read64_unlocked(dsp, offset);

	new = (old & (~mask)) | (value & mask);

	change = (old != new);
	if (change)
		hifi4dsp_dsp_smem_write64_unlocked(dsp, offset, new);

	return change;
}
EXPORT_SYMBOL(hifi4dsp_dsp_smem_update_bits64_unlocked);

int hifi4dsp_dsp_smem_update_bits(struct hifi4dsp_dsp *dsp, u32 offset,
				u32 mask, u32 value)
{
	unsigned long flags;
	bool change;

	spin_lock_irqsave(&dsp->spinlock, flags);
	change = hifi4dsp_dsp_smem_update_bits_unlocked(dsp,
				offset, mask, value);
	spin_unlock_irqrestore(&dsp->spinlock, flags);
	return change;
}
EXPORT_SYMBOL(hifi4dsp_dsp_smem_update_bits);

void hifi4dsp_dsp_smem_update_bits_forced(struct hifi4dsp_dsp *dsp,
		u32 offset, u32 mask, u32 value)
{
	unsigned long flags;

	spin_lock_irqsave(&dsp->spinlock, flags);
	hifi4dsp_dsp_smem_update_bits_forced_unlocked(dsp, offset, mask, value);
	spin_unlock_irqrestore(&dsp->spinlock, flags);
}
EXPORT_SYMBOL(hifi4dsp_dsp_smem_update_bits_forced);

int hifi4dsp_dsp_smem_update_bits64(struct hifi4dsp_dsp *dsp,
		u32 offset, u64 mask, u64 value)
{
	unsigned long flags;
	bool change;

	spin_lock_irqsave(&dsp->spinlock, flags);
	change = hifi4dsp_dsp_smem_update_bits64_unlocked(dsp,
				offset, mask, value);
	spin_unlock_irqrestore(&dsp->spinlock, flags);
	return change;
}
EXPORT_SYMBOL(hifi4dsp_dsp_smem_update_bits64);

int hifi4dsp_dsp_mailbox_init(struct hifi4dsp_dsp *dsp,
		u32 outbox_offset, size_t outbox_size,
		u32 inbox_offset, size_t inbox_size)
{
	dsp->mailbox.out_base = dsp->addr.smem + outbox_offset;
	dsp->mailbox.in_base =  dsp->addr.smem + inbox_offset;
	dsp->mailbox.out_size = outbox_size;
	dsp->mailbox.in_size =  inbox_size;
	return 0;
}
EXPORT_SYMBOL_GPL(hifi4dsp_dsp_mailbox_init);

void hifi4dsp_dsp_mailbox_outbox_write(struct hifi4dsp_dsp *dsp,
				void *message, size_t bytes)
{
	memcpy_toio(dsp->mailbox.out_base, message, bytes);
}
EXPORT_SYMBOL_GPL(hifi4dsp_dsp_mailbox_outbox_write);

void hifi4dsp_dsp_mailbox_outbox_read(struct hifi4dsp_dsp *dsp,
				void *message, size_t bytes)
{
	memcpy_fromio(message, dsp->mailbox.out_base, bytes);
}
EXPORT_SYMBOL_GPL(hifi4dsp_dsp_mailbox_outbox_read);

void hifi4dsp_dsp_mailbox_inbox_write(struct hifi4dsp_dsp *dsp,
				void *message, size_t bytes)
{
	memcpy_toio(dsp->mailbox.in_base, message, bytes);
}
EXPORT_SYMBOL_GPL(hifi4dsp_dsp_mailbox_inbox_write);

void hifi4dsp_dsp_mailbox_inbox_read(struct hifi4dsp_dsp *dsp,
				void *message, size_t bytes)
{
	memcpy_fromio(message, dsp->mailbox.in_base, bytes);
}
EXPORT_SYMBOL_GPL(hifi4dsp_dsp_mailbox_inbox_read);

int hifi4dsp_dsp_boot(struct hifi4dsp_dsp *dsp)
{
	if (dsp->ops->boot)
		dsp->ops->boot(dsp);
	pr_debug("%s done\n", __func__);
	return 0;
}
EXPORT_SYMBOL(hifi4dsp_dsp_boot);

void hifi4dsp_dsp_reset(struct hifi4dsp_dsp *dsp)
{
	if (dsp->ops->reset)
		dsp->ops->reset(dsp);
	pr_debug("%s done\n", __func__);
}
EXPORT_SYMBOL(hifi4dsp_dsp_reset);

void hifi4dsp_dsp_sleep(struct hifi4dsp_dsp *dsp)
{
	if (dsp->ops->sleep)
		dsp->ops->sleep(dsp);
}
EXPORT_SYMBOL(hifi4dsp_dsp_sleep);

int hifi4dsp_dsp_wake(struct hifi4dsp_dsp *dsp)
{
	if (dsp->ops->wake)
		return dsp->ops->wake(dsp);
	return 0;
}
EXPORT_SYMBOL(hifi4dsp_dsp_wake);

void hifi4dsp_dsp_dump(struct hifi4dsp_dsp *dsp)
{
	if (dsp->ops->dump)
		dsp->ops->dump(dsp);
}
EXPORT_SYMBOL(hifi4dsp_dsp_dump);

struct hifi4dsp_dsp *hifi4dsp_dsp_new(struct hifi4dsp_priv *priv,
		struct hifi4dsp_pdata *pdata,
		struct hifi4dsp_dsp_device *dsp_dev)
{
	int err = 0;
	struct hifi4dsp_dsp *dsp;

	dsp = kzalloc(sizeof(struct hifi4dsp_dsp), GFP_KERNEL);
	if (dsp == NULL)
		goto dsp_malloc_error;

	mutex_init(&dsp->mutex);
	spin_lock_init(&dsp->spinlock);
	spin_lock_init(&dsp->fw_spinlock);
	INIT_LIST_HEAD(&dsp->fw_list);

	dsp->id = pdata->id;
	dsp->irq = pdata->irq;
	dsp->major_id = MAJOR(priv->dev->devt);
	dsp->dev = priv->dev;
	dsp->pdata = pdata;
	dsp->priv = priv;
	dsp->ops = dsp_dev->ops;

	/* Initialise Audio DSP */
	if (dsp->ops->init) {
		err = dsp->ops->init(dsp, pdata);
		if (err < 0)
			return NULL;
	}

	/*Register the ISR here if necessary*/
	/*
	 * err = request_threaded_irq(dsp->irq, dsp->ops->irq_handler,
	 *	dsp_dev->thread, IRQF_SHARED, "HIFI4DSP", dsp);
	 * if (err)
	 *	goto irq_err;
	 */
	goto dsp_new_done;
	/*
	 * irq_err:
	 *	if (dsp->ops->free)
	 *		dsp->ops->free(dsp);
	 */
dsp_malloc_error:
dsp_new_done:
	return dsp;
}
EXPORT_SYMBOL(hifi4dsp_dsp_new);

MODULE_AUTHOR("Shuyu Li");
MODULE_DESCRIPTION("HiFi DSP Driver");
MODULE_LICENSE("GPL v2");
