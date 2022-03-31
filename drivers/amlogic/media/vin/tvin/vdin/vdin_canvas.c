// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/vin/tvin/vdin/vdin_canvas.c
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

/* Standard Linux headers */
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/cma.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/dma-contiguous.h>
#include <linux/delay.h>

/* Amlogic headers */
#include <linux/amlogic/media/vfm/vframe.h>
#ifdef CONFIG_AMLOGIC_TEE
#include <linux/amlogic/tee.h>
#endif

/* Local headers */
#include "../tvin_format_table.h"
#include "vdin_drv.h"
#include "vdin_canvas.h"
#include "vdin_ctl.h"
#include "vdin_dv.h"
/*the value depending on dts config mem limit
 *for skip two vframe case,need +2
 */
static unsigned int max_buf_num = VDIN_CANVAS_MAX_CNT;
static unsigned int min_buf_num = VDIN_CANVAS_MIN_CNT;
static unsigned int max_buf_width = VDIN_CANVAS_MAX_WIDTH_HD;
static unsigned int max_buf_height = VDIN_CANVAS_MAX_HEIGHT;
/* one frame max metadata size:32x280 bits = 1120bytes(0x460) */
unsigned int dolby_size_byte = K_DV_META_BUFF_SIZE;

module_param(max_buf_num, uint, 0664);
MODULE_PARM_DESC(max_buf_num, "vdin max buf num.\n");

module_param(min_buf_num, uint, 0664);
MODULE_PARM_DESC(min_buf_num, "vdin min buf num.\n");

#ifdef DEBUG_SUPPORT
module_param(max_buf_width, uint, 0664);
MODULE_PARM_DESC(max_buf_width, "vdin max buf width.\n");

module_param(max_buf_height, uint, 0664);
MODULE_PARM_DESC(max_buf_height, "vdin max buf height.\n");

module_param(dolby_size_byte, uint, 0664);
MODULE_PARM_DESC(dolby_size_byte, "dolby_size_byte.\n");
#endif

const unsigned int vdin_canvas_ids[2][VDIN_CANVAS_MAX_CNT] = {
	{
		0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b,
		0x2c, 0x2d, 0x2e, 0x2f, 0x30
	},
	{
		0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
		0x37, 0x38, 0x35, 0x36, 0x39
	},
};

//#define TEE_MEM_TYPE_VDIN	0x7

/*function:
 *	1.set canvas_max_w & canvas_max_h
 *	2.set canvas_max_size & canvas_max_num
 *	3.set canvas_id & canvas_addr
 */
void vdin_canvas_init(struct vdin_dev_s *devp)
{
	int i, canvas_id;
	unsigned int canvas_addr;
	int canvas_max_w = 0;
	int canvas_max_h = VDIN_CANVAS_MAX_HEIGHT;

	canvas_max_w = VDIN_CANVAS_MAX_WIDTH_HD << 1;

	devp->canvas_max_size = PAGE_ALIGN(canvas_max_w * canvas_max_h);
	devp->canvas_max_num  = devp->mem_size / devp->canvas_max_size;

	if (devp->canvas_max_num > VDIN_CANVAS_MAX_CNT)
		devp->canvas_max_num = VDIN_CANVAS_MAX_CNT;

	devp->mem_start = roundup(devp->mem_start, devp->canvas_align);
	pr_info("vdin.%d canvas initial table:\n", devp->index);
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

/*function:canvas_config when canvas_config_mode=1
 *	1.set canvas_w and canvas_h
 *	2.set canvas_max_size and canvas_max_num
 *	3.when dest_cfmt is TVIN_NV12/TVIN_NV21,
 *		buf width add canvas_w*canvas_h
 *based on parameters:
 *	format_convert/	source_bitdepth/
 *	v_active color_depth_mode/	prop.dest_cfmt
 */
void vdin_canvas_start_config(struct vdin_dev_s *devp)
{
	int i = 0;
	int canvas_id;
	unsigned long canvas_addr;
	unsigned int chroma_size = 0;
	unsigned int canvas_step = 1;
	unsigned int canvas_num = VDIN_CANVAS_MAX_CNT;
	unsigned int max_buffer_num = max_buf_num;

	switch (devp->format_convert) {
	case VDIN_FORMAT_CONVERT_YUV_YUV444:
	case VDIN_FORMAT_CONVERT_YUV_RGB:
	case VDIN_FORMAT_CONVERT_RGB_YUV444:
	case VDIN_FORMAT_CONVERT_RGB_RGB:
	case VDIN_FORMAT_CONVERT_YUV_GBR:
	case VDIN_FORMAT_CONVERT_YUV_BRG:
		if (devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH)
			devp->canvas_w = max_buf_width * VDIN_YUV444_10BIT_PER_PIXEL_BYTE;
		else
			devp->canvas_w = max_buf_height * VDIN_YUV444_8BIT_PER_PIXEL_BYTE;
		break;
	case VDIN_FORMAT_CONVERT_YUV_NV12:
	case VDIN_FORMAT_CONVERT_YUV_NV21:
	case VDIN_FORMAT_CONVERT_RGB_NV12:
	case VDIN_FORMAT_CONVERT_RGB_NV21:
		devp->canvas_w = max_buf_width;
		canvas_num = canvas_num / 2;
		canvas_step = 2;
		break;
	case VDIN_FORMAT_CONVERT_YUV_YUV422:
	case VDIN_FORMAT_CONVERT_RGB_YUV422:
	case VDIN_FORMAT_CONVERT_GBR_YUV422:
	case VDIN_FORMAT_CONVERT_BRG_YUV422:
		if (devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH &&
		    devp->full_pack == VDIN_422_FULL_PK_EN)
			devp->canvas_w = (max_buf_width * 5) / 2;
		else if ((devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH) &&
			 (devp->full_pack == VDIN_422_FULL_PK_DIS))
			devp->canvas_w = max_buf_width * VDIN_YUV422_10BIT_PER_PIXEL_BYTE;
		else
			devp->canvas_w = max_buf_width * VDIN_YUV422_8BIT_PER_PIXEL_BYTE;
		break;
	default:
		break;
	}

	/*backup before roundup*/
	devp->canvas_active_w = devp->canvas_w;
	if (devp->force_yuv444_malloc == 1) {
		/* 4k is not support 10 bit mode in order to save memory */
		if (devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH &&
		    !vdin_is_4k(devp))
			devp->canvas_w = devp->h_active * VDIN_YUV444_10BIT_PER_PIXEL_BYTE;
		else
			devp->canvas_w = devp->h_active * VDIN_YUV444_8BIT_PER_PIXEL_BYTE;
	}
	/*canvas_w must ensure divided exact by 256bit(32byte)*/
	devp->canvas_w = roundup(devp->canvas_w, devp->canvas_align);
	devp->canvas_h = devp->v_active;

	switch (devp->format_convert) {
	case VDIN_FORMAT_CONVERT_YUV_NV12:
	case VDIN_FORMAT_CONVERT_YUV_NV21:
	case VDIN_FORMAT_CONVERT_RGB_NV12:
	case VDIN_FORMAT_CONVERT_RGB_NV21:
		chroma_size = devp->canvas_w * devp->canvas_h / 2;
		break;
	default:
		break;
	}

	devp->canvas_max_size = PAGE_ALIGN(devp->canvas_w * devp->canvas_h + chroma_size);
	devp->canvas_max_num  = devp->mem_size / devp->canvas_max_size;

	devp->canvas_max_num = min(devp->canvas_max_num, canvas_num);
	devp->canvas_max_num = min(devp->canvas_max_num, max_buffer_num);

	if (devp->cma_config_en != 1 || !(devp->cma_config_flag & 0x100)) {
		/*use_reserved_mem or alloc_from_contiguous*/
		devp->mem_start = roundup(devp->mem_start, devp->canvas_align);
#ifdef VDIN_DEBUG
		pr_info("vdin%d canvas start configuration table:\n",
			devp->index);
#endif
		for (i = 0; i < devp->canvas_max_num; i++) {
			canvas_id = vdin_canvas_ids[devp->index][i * canvas_step];
			canvas_addr = devp->mem_start +
				devp->canvas_max_size * i;
			canvas_config(canvas_id, canvas_addr,
				devp->canvas_w, devp->canvas_h,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
			if (chroma_size)
				canvas_config(canvas_id + 1,
					      canvas_addr +
					      devp->canvas_w * devp->canvas_h,
					      devp->canvas_w,
					      devp->canvas_h / 2,
					      CANVAS_ADDR_NOWRAP,
					      CANVAS_BLKMODE_LINEAR);
#ifdef VDIN_DEBUG
			pr_info("\t%3d: 0x%lx-0x%lx %ux%u\n",
				canvas_id, canvas_addr,
				canvas_addr + devp->canvas_max_size,
				devp->canvas_w, devp->canvas_h);
#endif
		}
	} else if (devp->cma_config_flag & 0x100) {
#ifdef VDIN_DEBUG
		pr_info("vdin%d canvas start configuration table:\n",
			devp->index);
#endif
		for (i = 0; i < devp->canvas_max_num; i++) {
			devp->vf_mem_start[i] =
				roundup(devp->vf_mem_start[i], devp->canvas_align);
			canvas_id = vdin_canvas_ids[devp->index][i * canvas_step];
			canvas_addr = devp->vf_mem_start[i];
			canvas_config(canvas_id, canvas_addr,
				devp->canvas_w, devp->canvas_h,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
			if (chroma_size)
				canvas_config(canvas_id + 1,
					      canvas_addr +
					      devp->canvas_w * devp->canvas_h,
					      devp->canvas_w,
					      devp->canvas_h / 2,
					      CANVAS_ADDR_NOWRAP,
					      CANVAS_BLKMODE_LINEAR);
#ifdef VDIN_DEBUG
			pr_info("\t%3d: 0x%lx-0x%lx %ux%u\n",
				canvas_id, canvas_addr,
				canvas_addr + devp->canvas_max_size,
				devp->canvas_w, devp->canvas_h);
#endif
		}
	}
}

unsigned int vdin_get_canvas_num(struct vdin_dev_s *devp)
{
	return (devp->index == 0) ? VDIN0_CANVAS_MAX_CNT : VDIN1_CANVAS_MAX_CNT;
}

/*
 *this function used for configure canvas when canvas_config_mode=2
 *base on the input format
 *also used for input resolution over 1080p such as camera input 200M,500M
 *YUV422-8BIT:1pixel = 2byte;
 *YUV422-10BIT:1pixel = 3byte;
 *YUV422-10BIT-FULL_PACK:1pixel = 2.5byte;
 *YUV444-8BIT:1pixel = 3byte;
 *YUV444-10BIT:1pixel = 4byte
 */
void vdin_canvas_auto_config(struct vdin_dev_s *devp)
{
	int i = 0;
	int canvas_id;
	unsigned long canvas_addr;
	unsigned long chroma_canvas_addr;
	unsigned int chroma_size = 0;
	unsigned int canvas_step = 1;
	unsigned int canvas_num = VDIN_CANVAS_MAX_CNT;
	unsigned int max_buffer_num = max_buf_num;
	unsigned int h_active, v_active;

	if (devp->vf_mem_size_small) {
		h_active = devp->h_shrink_out;
		v_active = devp->v_shrink_out;
	} else {
		h_active = devp->h_active;
		v_active = devp->v_active;
	}

	switch (devp->format_convert) {
	case VDIN_FORMAT_CONVERT_YUV_YUV444:
	case VDIN_FORMAT_CONVERT_YUV_RGB:
	case VDIN_FORMAT_CONVERT_RGB_YUV444:
	case VDIN_FORMAT_CONVERT_RGB_RGB:
	case VDIN_FORMAT_CONVERT_YUV_GBR:
	case VDIN_FORMAT_CONVERT_YUV_BRG:
		if (devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH)
			devp->canvas_w = h_active * VDIN_YUV444_10BIT_PER_PIXEL_BYTE;
		else
			devp->canvas_w = h_active * VDIN_YUV444_8BIT_PER_PIXEL_BYTE;
		break;
	case VDIN_FORMAT_CONVERT_YUV_NV12:
	case VDIN_FORMAT_CONVERT_YUV_NV21:
	case VDIN_FORMAT_CONVERT_RGB_NV12:
	case VDIN_FORMAT_CONVERT_RGB_NV21:
		/* screencap case:S4/S4D cannot use the same canvas id */
		if (devp->dtdata->hw_ver == VDIN_HW_S4 ||
			devp->dtdata->hw_ver == VDIN_HW_S4D)
			canvas_num = vdin_get_canvas_num(devp);
		/* screencap case:end */
		canvas_num = canvas_num / 2;
		canvas_step = 2;
		devp->canvas_w = h_active;
		break;
	case VDIN_FORMAT_CONVERT_YUV_YUV422:
	case VDIN_FORMAT_CONVERT_RGB_YUV422:
	case VDIN_FORMAT_CONVERT_GBR_YUV422:
	case VDIN_FORMAT_CONVERT_BRG_YUV422:
		if (devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH) {
			if (devp->full_pack == VDIN_422_FULL_PK_EN)
				devp->canvas_w = (h_active * 5) / 2;
			else if (devp->full_pack == VDIN_422_FULL_PK_DIS)
				devp->canvas_w = h_active * VDIN_YUV422_10BIT_PER_PIXEL_BYTE;
		} else {
			devp->canvas_w = h_active * VDIN_YUV422_8BIT_PER_PIXEL_BYTE;
		}

		/* dw only support 8bit mode */
		if (devp->double_wr)
			devp->canvas_w = h_active * VDIN_YUV422_8BIT_PER_PIXEL_BYTE;

		break;
	default:
		break;
	}

	/*backup before roundup*/
	devp->canvas_active_w = devp->canvas_w;

	/* dw only support 8 bit mode */
	if (devp->force_yuv444_malloc == 1 && !devp->double_wr) {
		/* 4k is not support 10 bit mode in order to save memory */
		if (devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH &&
		    !vdin_is_4k(devp))
			devp->canvas_w = h_active * VDIN_YUV444_10BIT_PER_PIXEL_BYTE;
		else
			devp->canvas_w = h_active * VDIN_YUV444_8BIT_PER_PIXEL_BYTE;
	}
	/*canvas_w must ensure divided exact by 256bit(32byte)*/
	devp->canvas_w = roundup(devp->canvas_w, devp->canvas_align);
	devp->canvas_h = v_active;

	switch (devp->format_convert) {
	case VDIN_FORMAT_CONVERT_YUV_NV12:
	case VDIN_FORMAT_CONVERT_YUV_NV21:
	case VDIN_FORMAT_CONVERT_RGB_NV12:
	case VDIN_FORMAT_CONVERT_RGB_NV21:
		chroma_size = devp->canvas_w * devp->canvas_h / 2;
		break;
	default:
		break;
	}

	devp->canvas_max_size =
		PAGE_ALIGN(devp->canvas_w * devp->canvas_h + chroma_size);
	/* devp->canvas_max_num  = devp->mem_size / devp->canvas_max_size; */

	devp->canvas_max_num = min(devp->canvas_max_num, canvas_num);
	devp->canvas_max_num = min(devp->canvas_max_num, max_buffer_num);
	if (devp->canvas_max_num < devp->vf_mem_max_cnt) {
		pr_err("vdin%d canvas_max_num %d less vf_mem_max_cnt %d\n",
			devp->index, devp->canvas_max_num, devp->vf_mem_max_cnt);
	}
	devp->vf_mem_max_cnt = min(devp->vf_mem_max_cnt, devp->canvas_max_num);

#ifdef VDIN_DEBUG
	pr_info("vdin%d canvas auto configuration table:\n",
		devp->index);
#endif
	for (i = 0; i < devp->canvas_max_num; i++) {
		devp->vf_mem_start[i] =
			roundup(devp->vf_mem_start[i], devp->canvas_align);
		canvas_id = vdin_canvas_ids[devp->index][i * canvas_step];
		canvas_addr = devp->vf_mem_start[i];
		canvas_config(canvas_id, canvas_addr,
			      devp->canvas_w, devp->canvas_h,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		/* vdin v4l2:two non contiguous planes */
		if (devp->work_mode == VDIN_WORK_MD_V4L &&
			devp->v4l2_fmt.fmt.pix_mp.num_planes == 2) {
			chroma_canvas_addr = devp->vf_mem_c_start[i];
			if (devp->vf_mem_c_start[i] == 0)
				pr_err("vdin%d,addr for chroma is NULL!\n", devp->index);
		} else {
			chroma_canvas_addr = canvas_addr + devp->canvas_w * devp->canvas_h;
		}
		if (chroma_size)
			canvas_config(canvas_id + 1,
				      chroma_canvas_addr,
				      devp->canvas_w,
				      devp->canvas_h / 2,
				      CANVAS_ADDR_NOWRAP,
				      CANVAS_BLKMODE_LINEAR);
#ifdef VDIN_DEBUG
		pr_info("\t%3d: 0x%lx-0x%lx %ux%u\n",
			canvas_id, canvas_addr,
			canvas_addr + devp->canvas_max_size,
			devp->canvas_w, devp->canvas_h);
#endif
	}
}

#ifdef CONFIG_CMA
/* need to be static for pointer use in codec_mm */
static char vdin_name[6];

/* return val:1: fail;0: ok */
/* combined canvas and afbce memory */
unsigned int vdin_cma_alloc(struct vdin_dev_s *devp)
{
	unsigned int mem_size, h_size, v_size, frame_size, temp;
	int flags = CODEC_MM_FLAGS_CMA_FIRST | CODEC_MM_FLAGS_CMA_CLEAR |
		CODEC_MM_FLAGS_DMA;
	unsigned int max_buffer_num = min_buf_num;
	unsigned int i, j;
#ifdef CONFIG_AMLOGIC_TEE
	unsigned int res = 0;
#endif
	struct codec_mm_s *mem = NULL;

	if (devp->rdma_enable)
		max_buffer_num++;
	/*todo: need update if vf_skip_cnt used by other port*/
	if (devp->vfp->skip_vf_num &&
	    (IS_HDMI_SRC(devp->parm.port) || IS_TVAFE_SRC(devp->parm.port)))
		max_buffer_num += devp->vfp->skip_vf_num;

	if (max_buffer_num > max_buf_num)
		max_buffer_num = max_buf_num;

	/* if vfmem_cfg_num define in dts, use dts's setting */
	if (devp->frame_buff_num >= min_buf_num &&
	    devp->frame_buff_num <= max_buf_num)
		max_buffer_num = devp->frame_buff_num;

	if (!devp->index && devp->frame_buff_num < VDIN_CANVAS_INTERLACED_MIN_CNT &&
	    devp->fmt_info_p->scan_mode == TVIN_SCAN_MODE_INTERLACED)
		max_buffer_num = VDIN_CANVAS_INTERLACED_MIN_CNT;

	devp->canvas_max_num = max_buffer_num;
	devp->vf_mem_max_cnt = max_buffer_num;
	if (devp->cma_config_en == 0 || devp->cma_mem_alloc == 1) {
		pr_info("vdin%d %s use_reserved mem or cma already allocated (%d,%d)!!!\n",
			devp->index, __func__, devp->cma_config_en,
			devp->cma_mem_alloc);
		return 0;
	}

	/*pixels*/
	h_size = devp->h_active;
	v_size = devp->v_active;

	if (devp->canvas_config_mode == 1) {
		h_size = max_buf_width;
		v_size = max_buf_height;
	}
	if (devp->format_convert == VDIN_FORMAT_CONVERT_YUV_YUV444 ||
	    devp->format_convert == VDIN_FORMAT_CONVERT_YUV_RGB ||
	    devp->format_convert == VDIN_FORMAT_CONVERT_RGB_YUV444 ||
	    devp->format_convert == VDIN_FORMAT_CONVERT_RGB_RGB ||
	    devp->format_convert == VDIN_FORMAT_CONVERT_YUV_GBR ||
	    devp->format_convert == VDIN_FORMAT_CONVERT_YUV_BRG) {
		/* 4k is not support 10 bit mode in order to save memory
		 * up to 4k 444 8bit mode
		 */
		if (/*devp->source_bitdepth > VDIN_MIN_SOURCE_BIT_DEPTH &&*/
		    !vdin_is_4k(devp)) {
			h_size = roundup(h_size * VDIN_YUV444_10BIT_PER_PIXEL_BYTE,
				devp->canvas_align);
			devp->canvas_align_w = h_size / VDIN_YUV444_10BIT_PER_PIXEL_BYTE;
		} else {
			h_size = roundup(h_size * VDIN_YUV444_8BIT_PER_PIXEL_BYTE,
				devp->canvas_align);
			devp->canvas_align_w = h_size / VDIN_YUV444_8BIT_PER_PIXEL_BYTE;
		}
	} else if ((devp->format_convert == VDIN_FORMAT_CONVERT_YUV_NV12) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_YUV_NV21) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_NV12) ||
		(devp->format_convert == VDIN_FORMAT_CONVERT_RGB_NV21)) {
		h_size = roundup(h_size, devp->canvas_align);
		devp->canvas_align_w = h_size;
		/*todo change with canvas alloc!!*/
		/* nv21/nv12 only have 8bit mode */
	} else {
		/* 422 mode, 4k up to 444 8bit mode,
		 * other up to 10 bit mode
		 */
		if (devp->index == 0) {
			if (vdin_is_4k(devp)) {
				/*up to 444 8bit*/
				h_size = roundup(h_size * VDIN_YUV444_8BIT_PER_PIXEL_BYTE,
						 devp->canvas_align);
				devp->canvas_align_w = h_size / VDIN_YUV444_8BIT_PER_PIXEL_BYTE;
			} else {
				if (devp->force_malloc_yuv_422_to_444) {
					/*up to 444 10bit*/
					h_size = roundup(h_size * VDIN_YUV444_10BIT_PER_PIXEL_BYTE,
							 devp->canvas_align);
					devp->canvas_align_w = h_size /
						VDIN_YUV444_10BIT_PER_PIXEL_BYTE;
				} else {
					if (devp->full_pack == VDIN_422_FULL_PK_EN) {
						h_size = roundup((h_size * 5) / 2,
								 devp->canvas_align);
						devp->canvas_align_w = (h_size * 2) / 5;
					} else {
						temp = h_size * VDIN_YUV422_10BIT_PER_PIXEL_BYTE;
						h_size = roundup(temp, devp->canvas_align);
						devp->canvas_align_w = h_size /
							VDIN_YUV422_10BIT_PER_PIXEL_BYTE;
					}
				}
			}
		} else if (devp->index) {
			if (devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH) {
				if (devp->full_pack == VDIN_422_FULL_PK_EN) {
					h_size = roundup((h_size * 5) / 2, devp->canvas_align);
					devp->canvas_align_w = (h_size * 2) / 5;
				} else {
					h_size = roundup(h_size * VDIN_YUV422_10BIT_PER_PIXEL_BYTE,
							 devp->canvas_align);
					devp->canvas_align_w = h_size /
						VDIN_YUV422_10BIT_PER_PIXEL_BYTE;
				}
			} else {
				h_size = roundup(h_size * VDIN_YUV422_8BIT_PER_PIXEL_BYTE,
					devp->canvas_align);
				devp->canvas_align_w = h_size / VDIN_YUV422_8BIT_PER_PIXEL_BYTE;
			}
		}
	}

	/*1 frame bytes*/
	mem_size = h_size * v_size;
	/* for almost uncompressed pattern,garbage at bottom
	 * 1024x1658 is the worst case,each page wast 2160x3x256byte for 4096
	 * since every block must not be separated by 2 pages
	 */
	if (devp->afbce_valid)
		mem_size += 1024 * 1658;

	if (devp->format_convert >= VDIN_FORMAT_CONVERT_YUV_NV12 &&
	    devp->format_convert <= VDIN_FORMAT_CONVERT_RGB_NV21)
		mem_size = (mem_size * 3) / 2;
	devp->vf_mem_size = PAGE_ALIGN(mem_size)/* + dolby_size_byte*/;
	devp->vf_mem_size = roundup(devp->vf_mem_size, PAGE_SIZE);

	if (devp->double_wr || K_FORCE_HV_SHRINK) {
		if (devp->h_shrink_out < devp->h_active &&
		    devp->v_shrink_out < devp->v_active)
			devp->vf_mem_size_small = devp->vf_mem_size /
				devp->h_shrink_times / devp->v_shrink_times;
		else if (devp->h_shrink_out < devp->h_active)
			devp->vf_mem_size_small =
				devp->vf_mem_size / devp->h_shrink_times;
		else if (devp->v_shrink_out < devp->v_active)
			devp->vf_mem_size_small =
				devp->vf_mem_size / devp->v_shrink_times;
		else
			devp->vf_mem_size_small = 0;
	} else {
		devp->vf_mem_size_small = 0;
	}

	if (devp->vf_mem_size_small)
		devp->vf_mem_size_small = roundup(devp->vf_mem_size_small, PAGE_SIZE);

	/* frame is consist of the following part
	 * 1st, small frame for dw
	 * 2nd, big frame for dw, or normal frame for non-dw
	 * 3rd, header if afbce enabled
	 * 4th, table if afbce enabled
	 * ---------------------
	 * |    small frame    |
	 * |                   |
	 * |-------------------|
	 * |                   |
	 * |    normal frame   |
	 * |                   |
	 * |-------------------|
	 * |      header       |
	 * |-------------------|
	 * |      table        |
	 * |-------------------|
	 */
	if (devp->afbce_info) {
		if (devp->afbce_valid) {
			/* allocate mem according to resolution
			 * each block contains 32 * 4 pixels
			 * one block associated to one header(4 bytes)
			 * dolby has one page size, & each vframe aligned to page size
			 * (((h(align 32 pixel) * v(align 4 pixel)) / (32 * 4)) * 4)  + dolby
			 * block should align to,  H:2 blocks, V: 16 blocks
			 */
			devp->afbce_info->frame_head_size =
			PAGE_ALIGN((roundup(devp->h_active, 64) *
				    roundup(devp->v_active, 64)) / 32
				    /* + dolby_size_byte*/);

			/*((h * v * byte_per_pixel + dolby) / page_size) * 4(one address size)
			 * total max_buffer_num
			 */
			devp->afbce_info->frame_table_size =
				PAGE_ALIGN((devp->vf_mem_size * 4) / PAGE_SIZE);
		} else {
			devp->afbce_info->frame_head_size = 0;
			devp->afbce_info->frame_table_size = 0;
		}

		frame_size = devp->vf_mem_size_small + devp->vf_mem_size +
			devp->afbce_info->frame_head_size + devp->afbce_info->frame_table_size;

	} else {
		frame_size = devp->vf_mem_size_small + devp->vf_mem_size;
	}

	devp->frame_size = frame_size;

	/*total frames bytes*/
	mem_size = PAGE_ALIGN(frame_size) * max_buffer_num;
	mem_size = roundup(mem_size, PAGE_SIZE);

#ifdef CONFIG_AMLOGIC_TEE
	/* must align to 64k for secure protection
	 * always align for switch secure mode dynamically
	 */
	mem_size = roundup(mem_size, 64 * 1024);
#endif

	if (mem_size > devp->cma_mem_size) {
		pr_err("vdin[%d] warning: cma_mem_size (need %d, cur %d) is not enough!!!\n",
		       devp->index, mem_size, devp->cma_mem_size);
		/*mem_size = devp->cma_mem_size;*/
		/*devp->cma_mem_alloc = 0;*/
		/*return 1;*/
	}

	if (devp->cma_config_flag & MEM_ALLOC_FROM_CODEC) {
		if (devp->index == 0)
			strcpy(vdin_name, "vdin0");
		else if (devp->index == 1)
			strcpy(vdin_name, "vdin1");
	}

	if (devp->set_canvas_manual == 1 || devp->cfg_dma_buf) {
		for (i = 0; i < VDIN_CANVAS_MAX_CNT; i++) {
			if (vdin_set_canvas_addr[i].dma_buffer == 0)
				break;
			vdin_set_canvas_addr[i].paddr =
				roundup(vdin_set_canvas_addr[i].paddr,
					devp->canvas_align);
			if (devp->cfg_dma_buf)
				devp->mem_start = vdin_set_canvas_addr[i].paddr;
			devp->vf_mem_start[i] = vdin_set_canvas_addr[i].paddr;
			if (vdin_dbg_en)
				pr_info("vdin%d buf[%d] mem_start = 0x%lx, mem_size = 0x%x\n",
					devp->index, i,
					devp->vf_mem_start[i],
					vdin_set_canvas_addr[i].size);
		}
		/*real buffer number*/
		max_buffer_num = i;
		devp->canvas_max_num = max_buffer_num;
		devp->vf_mem_max_cnt = max_buffer_num;
		/*update to real total frames size*/
		if (i > 0)
			mem_size =
				vdin_set_canvas_addr[0].size * max_buffer_num;
		devp->mem_size = mem_size;
		devp->cma_mem_alloc = 1;
		pr_info("vdin%d keystone cma alloc %d buffers ok!\n",
			devp->index, devp->vf_mem_max_cnt);
		return 0;
	}

	if (devp->work_mode == VDIN_WORK_MD_V4L) {
		max_buffer_num       = devp->v4l2_req_buf_num;
		devp->canvas_max_num = devp->v4l2_req_buf_num;
		devp->vf_mem_max_cnt  = devp->v4l2_req_buf_num;
		pr_info("%s vdin%d max_buffer_num = %d!\n", __func__,
			devp->index, devp->vf_mem_max_cnt);
		return 0;
	}
	if (devp->cma_config_flag & MEM_ALLOC_DISCRETE) {
		for (i = 0; i < max_buffer_num; i++) {
			if (devp->cma_config_flag & MEM_ALLOC_FROM_CODEC) {
				/*add for 1g config, codec can't release mem in time*/
				for (j = 0; j < 20; j++) {
					mem = codec_mm_alloc(vdin_name,
						(frame_size / PAGE_SIZE) << PAGE_SHIFT, 0, flags);
					if (mem)
						devp->vf_mem_start[i] = mem->phy_addr;
					else
						devp->vf_mem_start[i] = 0;

					if (devp->vf_mem_start[i] == 0) {
						msleep(50);
						pr_err("alloc mem fail:50*%dms\n", j);
					} else {
						devp->vf_codec_mem[i] = mem;
						break;
					}
				}

				if (j >= 20) {
					pr_err("vdin%d buf[%d]codec alloc fail!!!\n",
					       devp->index, i);
					/*real buffer number*/
					max_buffer_num = i;
					devp->canvas_max_num = i;
					devp->vf_mem_max_cnt = i;
					/*update to real total frames size*/
					mem_size = PAGE_ALIGN(frame_size) * max_buffer_num;
					break;
				}
			} else {
				devp->vf_venc_pages[i] =
					dma_alloc_from_contiguous(&devp->this_pdev->dev,
								  frame_size >> PAGE_SHIFT, 0, 0);
				if (!devp->vf_venc_pages[i]) {
					pr_err("vdin%d cma mem undefined2.\n", devp->index);
					/*real buffer number*/
					max_buffer_num = i;
					devp->canvas_max_num = i;
					devp->vf_mem_max_cnt = i;
					/*update to real total frames size*/
					mem_size = PAGE_ALIGN(frame_size) * max_buffer_num;
					break;
				}
				devp->vf_mem_start[i] = page_to_phys(devp->vf_venc_pages[i]);
			}

			if (vdin_dbg_en)
				pr_info("vdin%d buf[%d] mem_start = 0x%lx, mem_size = 0x%x\n",
					devp->index, i,	devp->vf_mem_start[i], frame_size);
		}
	} else {
		if (devp->cma_config_flag & MEM_ALLOC_FROM_CODEC) {
			devp->mem_start =
				codec_mm_alloc_for_dma(vdin_name, mem_size / PAGE_SIZE, 0, flags);

			if (devp->mem_start == 0) {
				pr_err("vdin%d codec alloc fail!!!\n", devp->index);
				devp->cma_mem_alloc = 0;
				return 1;
			}
		} else {
			devp->venc_pages =
				dma_alloc_from_contiguous(&devp->this_pdev->dev,
							  mem_size >> PAGE_SHIFT, 0, 0);
			if (!devp->venc_pages) {
				devp->cma_mem_alloc = 0;
				pr_err("vdin%d cma mem undefined2.\n",
				       devp->index);
				return 1;
			}
			devp->mem_start = page_to_phys(devp->venc_pages);
		}

#ifdef CONFIG_AMLOGIC_TEE
		if (devp->secure_en) {
			devp->secure_handle = 0;
			res = tee_protect_mem_by_type(TEE_MEM_TYPE_VDIN,
						      devp->mem_start,
						      mem_size,
						      &devp->secure_handle);
			if (res)
				devp->mem_protected = 0;
			else
				devp->mem_protected = 1;
		}
#endif
		for (i = 0; i < max_buffer_num; i++) {
			devp->vf_mem_start[i] = devp->mem_start + frame_size * i;

			if (vdin_dbg_en)
				pr_info("vdin%d buf[%d] mem_start = 0x%lx, mem_size = 0x%x\n",
					devp->index, i,
					devp->vf_mem_start[i], frame_size);
		}

		pr_info("vdin%d mem_start = 0x%lx, mem_size = 0x%x\n",
			devp->index, devp->mem_start, mem_size);
	}

	devp->mem_size = mem_size;
	devp->cma_mem_alloc = 1;

	if (devp->afbce_info && devp->afbce_valid) {
		devp->afbce_info->frame_body_size = devp->vf_mem_size;

		for (i = 0; i < max_buffer_num; i++) {
			devp->afbce_info->fm_body_paddr[i] =
				devp->vf_mem_start[i] + devp->vf_mem_size_small;
			devp->afbce_info->fm_head_paddr[i] =
				devp->afbce_info->fm_body_paddr[i] + devp->vf_mem_size;

			devp->afbce_info->fm_table_paddr[i] =
				devp->afbce_info->fm_head_paddr[i] +
				devp->afbce_info->frame_head_size;

			if (vdin_dbg_en) {
				pr_info("vdin%d fm_head_paddr[%d] = 0x%lx, frame_head_size = 0x%x\n",
					devp->index, i,
					devp->afbce_info->fm_head_paddr[i],
					devp->afbce_info->frame_head_size);
				pr_info("vdin%d fm_table_paddr[%d]=0x%lx, frame_table_size = 0x%x\n",
					devp->index, i,
					devp->afbce_info->fm_table_paddr[i],
					devp->afbce_info->frame_table_size);
			}
		}
	}

	pr_info("vdin%d cma alloc %d buffers ok!\n", devp->index,
		devp->vf_mem_max_cnt);
	if (devp->vf_mem_max_cnt < min_buf_num) {
		pr_info("vdin%d cma alloc num too less need release\n", devp->index);
		vdin_cma_release(devp);
		return 1;
	}

	return 0;
}

/*this function used for codec cma release
 *	1.call codec_mm_free_for_dma() or
 *	  dma_release_from_contiguous() to release cma;
 *	2.reset mem_start & mem_size & cma_mem_alloc to 0;
 */
void vdin_cma_release(struct vdin_dev_s *devp)
{
	char vdin_name[6];
	unsigned int i;

	if (devp->cma_config_en == 0 || devp->cma_mem_alloc == 0) {
		if (devp->work_mode == VDIN_WORK_MD_V4L)
			pr_err("vdin%d %s v4l2 mode do not need do this (%d,%d)!!!\n",
				   devp->index, __func__, devp->cma_config_en,
				   devp->cma_mem_alloc);
		else
			pr_err("vdin%d %s fail for (%d,%d)!!!\n",
			       devp->index, __func__, devp->cma_config_en,
			       devp->cma_mem_alloc);
		return;
	}

	if (devp->cma_config_flag & MEM_ALLOC_FROM_CODEC) {
		if (devp->index == 0)
			strcpy(vdin_name, "vdin0");
		else if (devp->index == 1)
			strcpy(vdin_name, "vdin1");
	}

	if (devp->cma_config_flag & MEM_ALLOC_DISCRETE) {
		if (devp->cma_config_flag & MEM_ALLOC_FROM_CODEC) {
			/* canvas or afbce paddr */
			for (i = 0; i < devp->vf_mem_max_cnt; i++) {
				codec_mm_release(devp->vf_codec_mem[i], vdin_name);
				devp->vf_codec_mem[i] = NULL;
			}
			pr_info("vdin%d codec cma release ok!\n", devp->index);
		} else {
			for (i = 0; i < devp->vf_mem_max_cnt; i++)
				dma_release_from_contiguous(&devp->this_pdev->dev,
							    devp->vf_venc_pages[i],
							    devp->frame_size >> PAGE_SHIFT);

			pr_info("vdin%d cma release ok!\n", devp->index);
		}
	} else {
#ifdef CONFIG_AMLOGIC_TEE
		if (devp->secure_en && devp->mem_protected) {
			tee_unprotect_mem(devp->secure_handle);
			devp->mem_protected = 0;
		}
#endif
		if (devp->cma_config_flag & MEM_ALLOC_FROM_CODEC) {
			codec_mm_free_for_dma(vdin_name, devp->mem_start);
			pr_info("vdin%d codec cma release ok!\n", devp->index);
		} else {
			if (devp->venc_pages && devp->mem_size)
				dma_release_from_contiguous(&devp->this_pdev->dev,
							    devp->venc_pages,
							    devp->mem_size >> PAGE_SHIFT);

			pr_info("vdin%d cma release ok!\n", devp->index);
		}
	}

	devp->mem_start = 0;
	devp->mem_size = 0;
	devp->cma_mem_alloc = 0;
}

void vdin_set_mem_protect(struct vdin_dev_s *devp, unsigned int protect)
{
#ifdef CONFIG_AMLOGIC_TEE
	unsigned int res = 0;

	if (protect) {
		res = tee_protect_mem_by_type(TEE_MEM_TYPE_VDIN,
					      devp->mem_start,
					      devp->mem_size,
					      &devp->secure_handle);
		if (res)
			devp->mem_protected = 0;
		else
			devp->mem_protected = 1;
	} else {
		tee_unprotect_mem(devp->secure_handle);
		devp->mem_protected = 0;
	}
#endif
}

/*@20170823 new add for the case of csc change after signal stable*/
void vdin_cma_malloc_mode(struct vdin_dev_s *devp)
{
	unsigned int h_size, v_size, cma_mem_mode_flag = 0;

	h_size = devp->h_active;
	v_size = devp->v_active;

	switch (devp->format_convert) {
	case VDIN_FORMAT_CONVERT_YUV_YUV422:
	case VDIN_FORMAT_CONVERT_RGB_YUV422:
	case VDIN_FORMAT_CONVERT_GBR_YUV422:
	case VDIN_FORMAT_CONVERT_BRG_YUV422:
		if (devp->force_malloc_yuv_422_to_444 && devp->cma_mem_mode)
			cma_mem_mode_flag = 1;
		break;
	default:
		if (devp->cma_mem_mode)
			cma_mem_mode_flag = 1;
		break;
	}

	if (cma_mem_mode_flag == 0)
		return;

	if (h_size <= VDIN_YUV444_MAX_CMA_WIDTH &&
	    v_size <= VDIN_YUV444_MAX_CMA_HEIGHT)
		devp->force_yuv444_malloc = 1;
	else
		devp->force_yuv444_malloc = 0;

	if (devp->work_mode == VDIN_WORK_MD_V4L) {
		pr_info("%s vdin v4l2 mode,set force_yuv444_malloc to 0\n", __func__);
		devp->force_yuv444_malloc = 0;
	}
}
#endif

