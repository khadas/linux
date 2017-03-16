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

#include "../tvin_format_table.h"
#include "vdin_drv.h"
#include "vdin_canvas.h"
#ifndef VDIN_DEBUG
#undef  pr_info
#define pr_info(fmt, ...)
#endif

unsigned int max_buf_num = 4;
module_param(max_buf_num, uint, 0664);
MODULE_PARM_DESC(max_buf_num, "vdin max buf num.\n");

unsigned int max_buf_width = VDIN_CANVAS_MAX_WIDTH_HD;
module_param(max_buf_width, uint, 0664);
MODULE_PARM_DESC(max_buf_width, "vdin max buf width.\n");

unsigned int max_buf_height = VDIN_CANVAS_MAX_HEIGH;
module_param(max_buf_height, uint, 0664);
MODULE_PARM_DESC(max_buf_height, "vdin max buf height.\n");

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
	unsigned int max_bufffer_num = max_buf_num;

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
		/*when vdin output YUV444/RGB444,Di will bypass dynamic;
		4 frame buffer may not enough,result in frame loss;
		After test on gxtvbb(input 1080p60,output 4k42210bit),
		6 frame is enough.*/
		max_bufffer_num = max_buf_num + 2;
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
	devp->canvas_max_num = min(devp->canvas_max_num, max_bufffer_num);

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
	unsigned int max_bufffer_num = max_buf_num;

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
		/*when vdin output YUV444/RGB444,Di will bypass dynamic;
		4 frame buffer may not enough,result in frame loss;
		After test on gxtvbb(input 1080p60,output 4k42210bit),
		6 frame is enough.*/
		max_bufffer_num = max_buf_num + 2;
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
	devp->canvas_max_num = min(devp->canvas_max_num, max_bufffer_num);

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

