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
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 */
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include "decoder_dma_alloc.h"

void *decoder_dma_alloc_coherent(ulong *phandle, u32 size, dma_addr_t *phy_addr, const char *owner)
{
	void *virt_addr;

	virt_addr = codec_mm_dma_alloc_coherent(phandle, (ulong *)phy_addr, size, owner);

	return virt_addr;
}
EXPORT_SYMBOL(decoder_dma_alloc_coherent);

void decoder_dma_free_coherent(ulong handle, u32 size, void *vaddr, dma_addr_t phy_addr)
{
	codec_mm_dma_free_coherent(handle);
}
EXPORT_SYMBOL(decoder_dma_free_coherent);
