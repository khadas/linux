/*
* Copyright (C) 2017 Amlogic, Inc. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*
* Description:
*/
#include <linux/module.h>

#include "aml_vcodec_drv.h"
#include "aml_vcodec_util.h"
//#include "aml_vpu.h"

/* For encoder, this will enable logs in venc/*/
bool aml_vcodec_dbg;
EXPORT_SYMBOL(aml_vcodec_dbg);

/* The log level of v4l2 encoder or decoder driver.
 * That is, files under aml-vcodec/.
 */
int aml_v4l2_dbg_level;
EXPORT_SYMBOL(aml_v4l2_dbg_level);

void __iomem *aml_vcodec_get_reg_addr(struct aml_vcodec_ctx *data,
					unsigned int reg_idx)
{
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)data;

	if (!data || reg_idx >= NUM_MAX_VCODEC_REG_BASE) {
		aml_v4l2_err("Invalid arguments, reg_idx=%d", reg_idx);
		return NULL;
	}
	return ctx->dev->reg_base[reg_idx];
}
EXPORT_SYMBOL(aml_vcodec_get_reg_addr);

int aml_vcodec_mem_alloc(struct aml_vcodec_ctx *data,
			struct aml_vcodec_mem *mem)
{
	unsigned long size = mem->size;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)data;
	struct device *dev = &ctx->dev->plat_dev->dev;

	//mem->va = dma_alloc_coherent(dev, size, &mem->dma_addr, GFP_KERNEL);
	mem->va = codec_mm_dma_alloc_coherent(dev_name(dev), size,
			&mem->dma_addr, GFP_KERNEL, 0);
	if (!mem->va) {
		aml_v4l2_err("%s dma_alloc size=%ld failed!", dev_name(dev),
			     size);
		return -ENOMEM;
	}

	memset(mem->va, 0, size);

	aml_v4l2_debug(4, "[%d]  - va      = %p", ctx->id, mem->va);
	aml_v4l2_debug(4, "[%d]  - dma     = 0x%lx", ctx->id,
		       (unsigned long)mem->dma_addr);
	aml_v4l2_debug(4, "[%d]    size = 0x%lx", ctx->id, size);

	return 0;
}
EXPORT_SYMBOL(aml_vcodec_mem_alloc);

void aml_vcodec_mem_free(struct aml_vcodec_ctx *data,
			struct aml_vcodec_mem *mem)
{
	unsigned long size = mem->size;
	struct aml_vcodec_ctx *ctx = (struct aml_vcodec_ctx *)data;
	struct device *dev = &ctx->dev->plat_dev->dev;

	if (!mem->va) {
		aml_v4l2_err("%s dma_free size=%ld failed!", dev_name(dev),
			     size);
		return;
	}

	aml_v4l2_debug(4, "[%d]  - va      = %p", ctx->id, mem->va);
	aml_v4l2_debug(4, "[%d]  - dma     = 0x%lx", ctx->id,
		       (unsigned long)mem->dma_addr);
	aml_v4l2_debug(4, "[%d]    size = 0x%lx", ctx->id, size);

	dma_free_coherent(dev, size, mem->va, mem->dma_addr);
	mem->va = NULL;
	mem->dma_addr = 0;
	mem->size = 0;
}
EXPORT_SYMBOL(aml_vcodec_mem_free);

void aml_vcodec_set_curr_ctx(struct aml_vcodec_dev *dev,
	struct aml_vcodec_ctx *ctx)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->irqlock, flags);
	dev->curr_ctx = ctx;
	spin_unlock_irqrestore(&dev->irqlock, flags);
}
EXPORT_SYMBOL(aml_vcodec_set_curr_ctx);

struct aml_vcodec_ctx *aml_vcodec_get_curr_ctx(struct aml_vcodec_dev *dev)
{
	unsigned long flags;
	struct aml_vcodec_ctx *ctx;

	spin_lock_irqsave(&dev->irqlock, flags);
	ctx = dev->curr_ctx;
	spin_unlock_irqrestore(&dev->irqlock, flags);
	return ctx;
}
EXPORT_SYMBOL(aml_vcodec_get_curr_ctx);
