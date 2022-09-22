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
#ifndef _DECODER_DMA_ALLOC_H_
#define _DECODER_DMA_ALLOC_H_

void *decoder_dma_alloc_coherent(ulong *, u32, dma_addr_t *, const char *);
void decoder_dma_free_coherent(ulong, u32, void *, dma_addr_t);

#define BUF_OWNER(format, use) format##_##use

/*format*/
#define H264F	"h264"
#define H265F	"h265"
#define AV1F	"av1"
#define VP9F	"vp9"
#define AVSF	"avs"
#define AVS2F	"avs2"
#define MPEG12F	"mpeg12"
#define MPEG4F	"mpeg4"
#define MJPEGF	"mjpeg"

/*use*/
#define AUX		"aux"
#define LMEM	"lmem"
#define MMU		"mmu_map"
#define DWMMU	"dwmmu_map"

#endif
