/*
 * drivers/amlogic/tvin/vdin/vdin_canvas.c
 *
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
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


/* #include <mach/dmc.h> */
/* #include <linux/amlogic/amports/canvas.h> */
#include <linux/amlogic/amports/vframe.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/cma.h>
#include <linux/amlogic/codec_mm/codec_mm.h>
#include <linux/dma-contiguous.h>

#include "../tvin_format_table.h"
#include "vdin_drv.h"
#include "vdin_canvas.h"
#include "vdin_ctl.h"
/*the value depending on dts config mem limit
when vdin output YUV444/RGB444,Di will bypass dynamic;
4 frame buffer may not enough,result in frame loss;
After test on gxtvbb(input 1080p60,output 4k42210bit),6 frame is enough?
(input 4k,output 4k42210bit 5 frame is enough).*/
static unsigned int max_buf_num = 6;
module_param(max_buf_num, uint, 0664);
MODULE_PARM_DESC(max_buf_num, "vdin max buf num.\n");

static unsigned int max_buf_width = VDIN_CANVAS_MAX_WIDTH_HD;
module_param(max_buf_width, uint, 0664);
MODULE_PARM_DESC(max_buf_width, "vdin max buf width.\n");

static unsigned int max_buf_height = VDIN_CANVAS_MAX_HEIGH;
module_param(max_buf_height, uint, 0664);
MODULE_PARM_DESC(max_buf_height, "vdin max buf height.\n");
/* one frame max metadata size:32x280 bits = 1120bytes(0x460) */
unsigned int dolby_size_byte = PAGE_SIZE;
module_param(dolby_size_byte, uint, 0664);
MODULE_PARM_DESC(dolby_size_byte, "dolby_size_byte.\n");

const unsigned int vdin_canvas_ids[2][VDIN_CANVAS_MAX_CNT] = {
	{
		38, 39, 40, 41, 42,
		43, 44, 45, 46,
	},
	{
		47, 48, 49, 50, 51,
		52, 53, 54, 55,
	},
};

void vdin_canvas_init(struct vdin_dev_s *devp)
{
	int i, canvas_id;
	unsigned int canvas_addr;
	int canvas_max_w = 0;
	int canvas_max_h = VDIN_CANVAS_MAX_HEIGH;

	if (is_meson_g9tv_cpu())
		canvas_max_w = VDIN_CANVAS_MAX_WIDTH_UHD << 1;
	else
		canvas_max_w = VDIN_CANVAS_MAX_WIDTH_HD << 1;

	devp->canvas_max_size = PAGE_ALIGN(canvas_max_w*canvas_max_h);
	devp->canvas_max_num  = devp->mem_size / devp->canvas_max_size;
	if (devp->canvas_max_num > VDIN_CANVAS_MAX_CNT)
		devp->canvas_max_num = VDIN_CANVAS_MAX_CNT;

	devp->mem_start = roundup(devp->mem_start, 32);
	pr_info("vdin.%d cnavas initial table:\n", devp->index);
	for (i = 0; i < devp->canvas_max_num; i++) {
		canvas_id = vdin_canvas_ids[devp->index][i];
		canvas_addr = devp->mem_start + devp->canvas_max_size * i;

		canvas_config(canvas_id, canvas_addr,
			canvas_max_w, canvas_max_h,
			CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		pr_info("\t%d: 0x%x-0x%x  %dx%d (%d KB)\n",
			canvas_id, canvas_addr,
			(canvas_addr + devp->canvas_max_size),
			canvas_max_w, canvas_max_h,
			(devp->canvas_max_size >> 10));
	}
}

void vdin_canvas_start_config(struct vdin_dev_s *devp)
{
	int i = 0;
	int canvas_id;
	unsigned long canvas_addr;
	unsigned int chroma_size = 0;
	unsigned int canvas_step = 1;
	unsigned int canvas_num = VDIN_CANVAS_MAX_CNT;
	unsigned int max_buffer_num = max_buf_num;

	/* todo: if new add output YUV444 format,this place should add too!!*/
	if ((devp->format_convert == VDIN_FORMAT_CONVERT_YUV_YUV444) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_YUV_RGB) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_YUV444) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_RGB) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_YUV_GBR) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_YUV_BRG)) {
		if (devp->source_bitdepth > 8)
			devp->canvas_w = max_buf_width * 4;
		else
			devp->canvas_w = max_buf_height * 3;
	} else if ((devp->prop.dest_cfmt == TVIN_NV12) ||
		(devp->prop.dest_cfmt == TVIN_NV21)) {
		devp->canvas_w = max_buf_width;
		canvas_num = canvas_num/2;
		canvas_step = 2;
	} else{/*YUV422*/
		/* txl new add yuv422 pack mode:canvas-w=h*2*10/8*/
		if ((devp->source_bitdepth > 8) &&
		((devp->format_convert == VDIN_FORMAT_CONVERT_YUV_YUV422) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_YUV422) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_GBR_YUV422) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_BRG_YUV422)) &&
		(devp->color_depth_mode == 1))
			devp->canvas_w = (max_buf_width * 5)/2;
		else if ((devp->source_bitdepth > 8) &&
			(devp->color_depth_mode == 0))
			devp->canvas_w = max_buf_width * 3;
		else
			devp->canvas_w = max_buf_width * 2;
	}
	/*backup before roundup*/
	devp->canvas_active_w = devp->canvas_w;
	/*canvas_w must ensure divided exact by 256bit(32byte)*/
	devp->canvas_w = roundup(devp->canvas_w, 32);
	devp->canvas_h = devp->v_active;

	if ((devp->prop.dest_cfmt == TVIN_NV12) ||
		(devp->prop.dest_cfmt == TVIN_NV21))
		chroma_size = devp->canvas_w*devp->canvas_h/2;

	devp->canvas_max_size = PAGE_ALIGN(devp->canvas_w*
			devp->canvas_h+chroma_size);
	devp->canvas_max_num  = devp->mem_size / devp->canvas_max_size;

	devp->canvas_max_num = min(devp->canvas_max_num, canvas_num);
	devp->canvas_max_num = min(devp->canvas_max_num, max_buffer_num);

	devp->mem_start = roundup(devp->mem_start, 32);
#ifdef VDIN_DEBUG
	pr_info("vdin%d cnavas auto configuration table:\n", devp->index);
#endif
	for (i = 0; i < devp->canvas_max_num; i++) {
		canvas_id = vdin_canvas_ids[devp->index][i*canvas_step];
		canvas_addr = devp->mem_start + devp->canvas_max_size * i;
		canvas_config(canvas_id, canvas_addr,
			devp->canvas_w, devp->canvas_h,
			CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		if (chroma_size)
			canvas_config(canvas_id+1,
				canvas_addr+devp->canvas_w*devp->canvas_h,
				devp->canvas_w,
				devp->canvas_h/2,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
#ifdef VDIN_DEBUG
		pr_info("\t%3d: 0x%lx-0x%lx %ux%u\n",
			canvas_id, canvas_addr,
			canvas_addr + devp->canvas_max_size,
			devp->canvas_w, devp->canvas_h);
#endif
	}
}

/*
*this function used for configure canvas base on the input format
*also used for input resalution over 1080p such as camera input 200M,500M
*YUV422-8BIT:1pixel = 2byte;
*YUV422-10BIT:1pixel = 3byte;
*YUV422-10BIT-FULLPACK:1pixel = 2.5byte;
*YUV444-8BIT:1pixel = 3byte;
*YUV444-10BIT:1pixel = 4bypte
*/
void vdin_canvas_auto_config(struct vdin_dev_s *devp)
{
	int i = 0;
	int canvas_id;
	unsigned long canvas_addr;
	unsigned int chroma_size = 0;
	unsigned int canvas_step = 1;
	unsigned int canvas_num = VDIN_CANVAS_MAX_CNT;
	unsigned int max_buffer_num = max_buf_num;

	/* todo: if new add output YUV444 format,this place should add too!!*/
	if ((devp->format_convert == VDIN_FORMAT_CONVERT_YUV_YUV444) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_YUV_RGB) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_YUV444) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_RGB) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_YUV_GBR) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_YUV_BRG)) {
		if (devp->source_bitdepth > 8)
			devp->canvas_w = devp->h_active * 4;
		else
			devp->canvas_w = devp->h_active * 3;
	} else if ((devp->prop.dest_cfmt == TVIN_NV12) ||
		(devp->prop.dest_cfmt == TVIN_NV21)) {
		canvas_num = canvas_num/2;
		canvas_step = 2;
		devp->canvas_w = devp->h_active;
		/* nv21/nv12 only have 8bit mode */
	} else {/*YUV422*/
		/* txl new add yuv422 pack mode:canvas-w=h*2*10/8*/
		if ((devp->source_bitdepth > 8) &&
		((devp->format_convert == VDIN_FORMAT_CONVERT_YUV_YUV422) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_YUV422) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_GBR_YUV422) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_BRG_YUV422)) &&
		(devp->color_depth_mode == 1))
			devp->canvas_w = (devp->h_active * 5)/2;
		else if ((devp->source_bitdepth > 8) &&
			(devp->color_depth_mode == 0))
			devp->canvas_w = devp->h_active * 3;
		else
			devp->canvas_w = devp->h_active * 2;
	}
	/*backup before roundup*/
	devp->canvas_active_w = devp->canvas_w;
	/*canvas_w must ensure divided exact by 256bit(32byte)*/
	devp->canvas_w = roundup(devp->canvas_w, 32);
	devp->canvas_h = devp->v_active;

	if ((devp->prop.dest_cfmt == TVIN_NV12) ||
		(devp->prop.dest_cfmt == TVIN_NV21))
		chroma_size = devp->canvas_w*devp->canvas_h/2;

	devp->canvas_max_size = PAGE_ALIGN(devp->canvas_w*
			devp->canvas_h+chroma_size);
	devp->canvas_max_num  = devp->mem_size / devp->canvas_max_size;

	devp->canvas_max_num = min(devp->canvas_max_num, canvas_num);
	devp->canvas_max_num = min(devp->canvas_max_num, max_buffer_num);

	devp->mem_start = roundup(devp->mem_start, 32);
#ifdef VDIN_DEBUG
	pr_info("vdin%d cnavas auto configuration table:\n", devp->index);
#endif
	for (i = 0; i < devp->canvas_max_num; i++) {
		canvas_id = vdin_canvas_ids[devp->index][i*canvas_step];
		canvas_addr = devp->mem_start + devp->canvas_max_size * i;
		canvas_config(canvas_id, canvas_addr,
			devp->canvas_w, devp->canvas_h,
			CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		if (chroma_size)
			canvas_config(canvas_id+1,
				canvas_addr+devp->canvas_w*devp->canvas_h,
				devp->canvas_w,
				devp->canvas_h/2,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
#ifdef VDIN_DEBUG
		pr_info("\t%3d: 0x%lx-0x%lx %ux%u\n",
			canvas_id, canvas_addr,
			canvas_addr + devp->canvas_max_size,
			devp->canvas_w, devp->canvas_h);
#endif
	}
}

#ifdef CONFIG_CMA
/* return val:1: fail;0: ok */
unsigned int vdin_cma_alloc(struct vdin_dev_s *devp)
{
	char vdin_name[5];
	unsigned int mem_size, h_size, v_size;
	int flags = CODEC_MM_FLAGS_CMA_FIRST|CODEC_MM_FLAGS_CMA_CLEAR|
		CODEC_MM_FLAGS_CPU;
	unsigned int max_buffer_num = 4;
	if (devp->rdma_enable)
		max_buffer_num++;
	/*todo: need update if vf_skip_cnt used by other port*/
	if (vf_skip_cnt &&
		((devp->parm.port >= TVIN_PORT_HDMI0) &&
			(devp->parm.port <= TVIN_PORT_HDMI7)))
		max_buffer_num++;
	if (max_buffer_num > max_buf_num)
		max_buffer_num = max_buf_num;

	if ((devp->cma_config_en == 0) ||
		(devp->cma_mem_alloc[devp->index] == 1)) {
		pr_err(KERN_ERR "\nvdin%d %s fail for (%d,%d)!!!\n",
			devp->index, __func__, devp->cma_config_en,
			devp->cma_mem_alloc[devp->index]);
		return 1;
	}
	h_size = devp->h_active;
	v_size = devp->v_active;
	if (devp->canvas_config_mode == 1) {
		h_size = max_buf_width;
		v_size = max_buf_height;
	}
	if ((devp->format_convert == VDIN_FORMAT_CONVERT_YUV_YUV444) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_YUV_RGB) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_YUV444) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_RGB) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_YUV_GBR) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_YUV_BRG)) {
		if (devp->source_bitdepth > 8) {
			h_size = roundup(h_size * 4, 32);
			devp->canvas_alin_w = h_size / 4;
		} else {
			h_size = roundup(h_size * 3, 32);
			devp->canvas_alin_w = h_size / 3;
		}
	} else if ((devp->format_convert == VDIN_FORMAT_CONVERT_YUV_NV12) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_YUV_NV21) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_NV12) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_NV21)) {
		h_size = roundup(h_size, 32);
		devp->canvas_alin_w = h_size;
		/*todo change with canvas alloc!!*/
		/* nv21/nv12 only have 8bit mode */
	} else {
		/* txl new add mode yuv422 pack mode:canvas-w=h*2*10/8
		*canvas_w must ensure divided exact by 256bit(32byte*/
		if ((devp->source_bitdepth > 8) &&
		((devp->format_convert == VDIN_FORMAT_CONVERT_YUV_YUV422) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_YUV422) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_GBR_YUV422) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_BRG_YUV422)) &&
		(devp->color_depth_mode == 1)) {
			h_size = roundup((h_size * 5)/2, 32);
			devp->canvas_alin_w = (h_size * 2) / 5;
		} else if ((devp->source_bitdepth > 8) &&
			(devp->color_depth_mode == 0)) {
			h_size = roundup(h_size * 3, 32);
			devp->canvas_alin_w = h_size / 3;
		} else {
			h_size = roundup(h_size * 2, 32);
			devp->canvas_alin_w = h_size / 2;
		}
	}
	mem_size = h_size * v_size;
	if ((devp->format_convert >= VDIN_FORMAT_CONVERT_YUV_NV12) ||
		(devp->format_convert <= VDIN_FORMAT_CONVERT_RGB_NV21))
		mem_size = (mem_size * 3)/2;
	mem_size = PAGE_ALIGN(mem_size) * max_buffer_num +
		dolby_size_byte * max_buffer_num;
	mem_size = (mem_size/PAGE_SIZE + 1)*PAGE_SIZE;
	if (mem_size > devp->cma_mem_size[devp->index])
		mem_size = devp->cma_mem_size[devp->index];
	if (devp->cma_config_flag == 1) {
		if (devp->index == 0)
			strcpy(vdin_name, "vdin0");
		else if (devp->index == 1)
			strcpy(vdin_name, "vdin1");
		devp->mem_start = codec_mm_alloc_for_dma(vdin_name,
			mem_size/PAGE_SIZE, 0, flags);
		devp->mem_size = mem_size;
		if (devp->mem_start == 0) {
			pr_err(KERN_ERR "\nvdin%d codec alloc fail!!!\n",
				devp->index);
			devp->cma_mem_alloc[devp->index] = 0;
			return 1;
		} else {
			devp->cma_mem_alloc[devp->index] = 1;
			pr_info("vdin%d mem_start = 0x%lx, mem_size = 0x%x\n",
				devp->index, devp->mem_start, devp->mem_size);
			pr_info("vdin%d codec cma alloc ok!\n", devp->index);
		}
	} else if (devp->cma_config_flag == 0) {
		devp->venc_pages[devp->index] = dma_alloc_from_contiguous(
			&(devp->this_pdev[devp->index]->dev),
			devp->cma_mem_size[devp->index] >> PAGE_SHIFT, 0);
		if (devp->venc_pages) {
			devp->mem_start =
				page_to_phys(devp->venc_pages[devp->index]);
			devp->mem_size  = mem_size;
			devp->cma_mem_alloc[devp->index] = 1;
			pr_info("vdin%d mem_start = 0x%lx, mem_size = 0x%x\n",
				devp->index, devp->mem_start, devp->mem_size);
			pr_info("vdin%d cma alloc ok!\n", devp->index);
		} else {
			devp->cma_mem_alloc[devp->index] = 0;
			pr_err(KERN_ERR "\nvdin%d cma mem undefined2.\n",
				devp->index);
			return 1;
		}
	}
	return 0;
}

void vdin_cma_release(struct vdin_dev_s *devp)
{
	char vdin_name[5];
	if ((devp->cma_config_en == 0) ||
		(devp->cma_mem_alloc[devp->index] == 0)) {
		pr_err(KERN_ERR "\nvdin%d %s fail for (%d,%d)!!!\n",
			devp->index, __func__, devp->cma_config_en,
			devp->cma_mem_alloc[devp->index]);
		return;
	}
	if ((devp->cma_config_flag == 1) && devp->mem_start) {
		if (devp->index == 0)
			strcpy(vdin_name, "vdin0");
		else if (devp->index == 1)
			strcpy(vdin_name, "vdin1");
		codec_mm_free_for_dma(vdin_name, devp->mem_start);
		pr_info("vdin%d codec cma release ok!\n", devp->index);
	} else if (devp->venc_pages[devp->index]
		&& devp->cma_mem_size[devp->index]
		&& (devp->cma_config_flag == 0)) {
		dma_release_from_contiguous(
			&(devp->this_pdev[devp->index]->dev),
			devp->venc_pages[devp->index],
			devp->cma_mem_size[devp->index] >> PAGE_SHIFT);
		pr_info("vdin%d cma release ok!\n", devp->index);
	} else {
		pr_err(KERN_ERR "\nvdin%d %s fail for (%d,%d,0x%lx)!!!\n",
			devp->index, __func__, devp->cma_mem_size[devp->index],
			devp->cma_config_flag, devp->mem_start);
	}
	devp->mem_start = 0;
	devp->mem_size = 0;
	devp->cma_mem_alloc[devp->index] = 0;
}
#endif

