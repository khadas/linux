/*
 * drivers/amlogic/media/video_sink/amvideocap.c
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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/atomic.h>
#include <linux/bug.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/old_cpu_version.h>
#include <linux/mutex.h>
#include <linux/amlogic/media/video_sink/amvideocap.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/delay.h>
#include <linux/time.h>

#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/dma-mapping.h>

#include <linux/amlogic/media/codec_mm/codec_mm.h>

#include "../../media_modules/stream_input/amports/amports_priv.h"
#include "amvideocap_priv.h"
/*
#if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6
#include <mach/mod_gate.h>
#endif
*/

#ifdef CONFIG_PM
#include <linux/pm.h>
#endif

#define DRIVER_NAME "amvideocap"
#define MODULE_NAME "amvideocap"
#define DEVICE_NAME "amvideocap"

#define CAP_WIDTH_MAX      3840
#define CAP_HEIGHT_MAX     2160

MODULE_DESCRIPTION("Video Frame capture");
MODULE_AUTHOR("amlogic-bj");
MODULE_LICENSE("GPL");

#define AMCAP_MAX_OPEND 16
struct amvideocap_global_data {
	struct class *class;
	struct device *dev;
	struct device *micro_dev;
	int major;
	unsigned long phyaddr;
	unsigned long vaddr;
	unsigned long size;
	int opened_cnt;
	int flags;
	struct mutex lock;
	struct video_frame_info want;
	u64 wait_max_ms;
};
static struct amvideocap_global_data amvideocap_gdata;
static struct ge2d_context_s *ge2d_amvideocap_context;
static inline struct amvideocap_global_data *getgctrl(void)
{
	return &amvideocap_gdata;
}

static int use_cma;
#ifdef CONFIG_CMA
static struct platform_device *amvideocap_pdev;
static int cma_max_size;
#define CMA_NAME "amvideocap"
#endif
#define gLOCK() mutex_lock(&(getgctrl()->lock))
#define gUNLOCK() mutex_unlock(&(getgctrl()->lock))
#define gLOCKINIT() mutex_init(&(getgctrl()->lock))

/*********************************************************
 * /dev/amvideo APIs
 *********************************************************/
static int amvideocap_open(struct inode *inode, struct file *file)
{
	struct amvideocap_private *priv;
	gLOCK();
#ifdef CONFIG_CMA
	if (use_cma && amvideocap_pdev) {
		unsigned long phybufaddr;
		phybufaddr = codec_mm_alloc_for_dma(CMA_NAME,
				cma_max_size * SZ_1M / PAGE_SIZE,
				4 + PAGE_SHIFT, CODEC_MM_FLAGS_CPU);
		/* pr_err("%s: codec_mm_alloc_for_dma:%p\n",
				__func__, (void *)phybufaddr);
		*/
		amvideocap_register_memory((unsigned char *)phybufaddr,
					cma_max_size * SZ_1M);
	}
#endif
	if (!getgctrl()->phyaddr) {
		gUNLOCK();
		pr_err("Error,no memory have register for amvideocap\n");
		return -ENOMEM;
	}
#ifdef CONFIG_CMA
	if (!getgctrl()->vaddr) {
		getgctrl()->vaddr = (unsigned long)codec_mm_phys_to_virt(
						getgctrl()->phyaddr);
		if (!getgctrl()->vaddr) {
			pr_err("%s: failed to remap y addr\n", __func__);
			gUNLOCK();
			return -ENOMEM;
		}
	}
#endif
	if (getgctrl()->opened_cnt > AMCAP_MAX_OPEND) {
		gUNLOCK();
		pr_err("Too Many opend video cap files\n");
		return -EMFILE;
	}
	priv = kmalloc(sizeof(struct amvideocap_private), GFP_KERNEL);
	if (!priv) {
		gUNLOCK();
		pr_err("alloc memory failed for amvideo cap\n");
		return -ENOMEM;
	}
	memset(priv, 0, sizeof(struct amvideocap_private));
	getgctrl()->opened_cnt++;
	gUNLOCK();
	file->private_data = priv;
	priv->phyaddr = getgctrl()->phyaddr;
	priv->vaddr = (u8 *) getgctrl()->vaddr;
	priv->want = getgctrl()->want;
	priv->src_rect.x = -1;
	priv->src_rect.y = -1;
	priv->src_rect.width = -1;
	priv->src_rect.height = -1;
	return 0;
}

static int amvideocap_release(struct inode *inode, struct file *file)
{
	struct amvideocap_private *priv = file->private_data;
	kfree(priv);
#ifdef CONFIG_CMA
	if (use_cma && amvideocap_pdev) {
		codec_mm_free_for_dma(CMA_NAME, getgctrl()->phyaddr);
		/*
		   pr_err("%s: codec_mm_free_for_dma:%p\n", __func__,
		   (void *)getgctrl()->phyaddr);
		 */
	}
#endif
	gLOCK();
	getgctrl()->opened_cnt--;

	gUNLOCK();
	return 0;
}

static int amvideocap_format_to_byte4pix(int fmt)
{
	switch (fmt) {
	case GE2D_FORMAT_S16_RGB_565:
		return 2;
	case GE2D_FORMAT_S24_BGR:
		return 3;
	case GE2D_FORMAT_S24_RGB:
		return 3;
	case GE2D_FORMAT_S32_ABGR:
		return 4;
	case GE2D_FORMAT_S32_RGBA:
		return 4;
	case GE2D_FORMAT_S32_BGRA:
		return 4;
	case GE2D_FORMAT_S32_ARGB:
		return 4;
	default:
		return 4;
	}
};

static int amvideocap_capture_get_frame
(struct amvideocap_private *priv, struct vframe_s **vf, int *cur_index)
{
	int ret;
	ret = ext_get_cur_video_frame(vf, cur_index);
	return ret;
}

static int amvideocap_capture_put_frame
(struct amvideocap_private *priv, struct vframe_s *vf)
{
	return ext_put_video_frame(vf);
}

static int amvideocap_get_input_format(struct vframe_s *vf)
{
	int format = GE2D_FORMAT_M24_NV21;
	/* pr_debug("vf->type:0x%x\n", vf->type); */

	if ((vf->type & VIDTYPE_VIU_422) == VIDTYPE_VIU_422) {
		pr_debug
		("*****************Into VIDTYPE_VIU_422******************\n");
		format = GE2D_FORMAT_S16_YUV422;
	} else if ((vf->type & VIDTYPE_VIU_444) == VIDTYPE_VIU_444) {
		pr_debug
		("*****************Into VIDTYPE_VIU_444******************\n");
		format = GE2D_FORMAT_S24_YUV444;
	} else if ((vf->type & VIDTYPE_VIU_NV21) == VIDTYPE_VIU_NV21) {
		pr_debug
		("****************Into VIDTYPE_VIU_NV21******************\n");
		format = GE2D_FORMAT_M24_NV21;
	}
	return format;
}

static ssize_t amvideocap_YUV_to_RGB(
	struct amvideocap_private *priv,
	u32 cur_index, int w, int h,
	struct vframe_s *vf, int outfmt)
{
	struct config_para_ex_s ge2d_config;
	struct canvas_s cs0, cs1, cs2, cd;

	void __iomem *psrc;
	void __iomem *pdst;
	int temp_cma_buf_size = 0;
	unsigned long phybufaddr_8bit = 0;
	struct timeval start, end;
	unsigned long time_use;
	int ret = 0;
	struct canvas_s temp_cs0, temp_cs1, temp_cs2;
	int temp_canvas_idx = -1;
	int temp_y_index = -1;
	int temp_u_index = -1;
	int temp_v_index = -1;
	unsigned char buf1[16];
	unsigned char buf2[16];
	unsigned char buf3[16];
	unsigned char buf_in[48];
	unsigned char buf_out[32];
	unsigned char tmp_buf[8];
	unsigned char in_cursor;
	unsigned char out_cursor;
	unsigned long read_size = 0;
	unsigned int i;
	unsigned char *line_start;
	unsigned long counter = 0;
	unsigned int w_align;
	unsigned int h_align;
	unsigned char tmp_char1;
	int height_after_di;

	const char *amvideocap_owner = "amvideocapframe";
	int canvas_idx = canvas_pool_map_alloc_canvas(amvideocap_owner);
	int y_index = cur_index & 0xff;
	int u_index = (cur_index >> 8) & 0xff;
	int v_index = (cur_index >> 16) & 0xff;
	int input_x, input_y, input_width, input_height, intfmt;
	memset(&ge2d_config, 0, sizeof(struct config_para_ex_s));
	intfmt = amvideocap_get_input_format(vf);

	if (((vf->bitdepth & BITDEPTH_Y10)) &&
		(intfmt == GE2D_FORMAT_S16_YUV422) &&
		(get_cpu_type() < MESON_CPU_MAJOR_ID_TXL)) {
		temp_canvas_idx =
			canvas_pool_map_alloc_canvas(amvideocap_owner);
		if (temp_canvas_idx < 0) {
			pr_err("alloc temp_canvas_idx failed");
			return -1;
		}
		temp_y_index = temp_canvas_idx & 0xff;
		temp_u_index = (temp_canvas_idx >> 8) & 0xff;
		temp_v_index = (temp_canvas_idx >> 16) & 0xff;
	}
	/* /unsigned long RGB_phy_addr=getgctrl()->phyaddr; */

	if (!priv->phyaddr) {
		pr_err("%s: failed to alloc y addr\n", __func__);
		return -1;
	}
	pr_debug("RGB_phy_addr:%x\n", (unsigned int)priv->phyaddr);

	if (vf == NULL) {
		pr_err("%s: vf is NULL\n", __func__);
		return -1;
	}

	canvas_config(canvas_idx, priv->phyaddr,
				  w * amvideocap_format_to_byte4pix(outfmt), h,
				  CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
	if (priv->src_rect.x < 0 || priv->src_rect.x > vf->width)
		input_x = 0;
	else
		input_x = priv->src_rect.x;

	if (priv->src_rect.y < 0 || priv->src_rect.y > vf->height)
		input_y = 0;
	else
		input_y = priv->src_rect.y;

	if (priv->src_rect.width < 0)
		input_width = vf->width;
	else if ((priv->src_rect.x + priv->src_rect.width) > vf->width) {
		input_width =
			priv->src_rect.x + priv->src_rect.width - vf->width;
	} else
		input_width = priv->src_rect.width;

	if (priv->src_rect.height < 0)
		input_height = vf->height;
	else if ((priv->src_rect.y + priv->src_rect.height) > vf->height) {
		input_height =
			priv->src_rect.y + priv->src_rect.height - vf->height;
	} else
		input_height = priv->src_rect.height;

	height_after_di = vf->height;
	if (((vf->bitdepth & BITDEPTH_Y10)) &&
		((intfmt == GE2D_FORMAT_S16_YUV422) ||
		((intfmt == GE2D_FORMAT_S24_YUV444) &&
		(get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL)))) {
		pr_debug("input_height = %d , vf->type_original = %x\n" ,
			input_height, vf->type_original);
		if ((vf->source_type == VFRAME_SOURCE_TYPE_HDMI) ||
			(vf->source_type == VFRAME_SOURCE_TYPE_CVBS) ||
			(vf->source_type == VFRAME_SOURCE_TYPE_TUNER)) {
			if (vf->type_original & VIDTYPE_INTERLACE) {
				height_after_di = vf->height >> 1;
				input_height >>= 1;
			} else {
				height_after_di = vf->height;
			}
		} else {
			/*local playback and DTV*/
			pr_debug("vf->prog_proc_config = %d",
				vf->prog_proc_config);
			if ((!vf->prog_proc_config) &&
				(!(vf->type_original & VIDTYPE_INTERLACE))) {
				height_after_di = vf->height;
			} else {
				height_after_di = vf->height >> 1;
				input_height >>= 1;
			}
		}
	} else if (is_meson_g9tv_cpu() || is_meson_gxtvbb_cpu()) {
		if (vf->type_original & VIDTYPE_INTERLACE)
			input_height = input_height / 2;
	} else {
		if (intfmt == GE2D_FORMAT_S16_YUV422)
			input_height = input_height / 2;
	}
	ge2d_config.alu_const_color = 0;
	ge2d_config.bitmask_en = 0;
	ge2d_config.src1_gb_alpha = 0;
	ge2d_config.dst_xy_swap = 0;

	canvas_read(y_index, &cs0);
	canvas_read(u_index, &cs1);
	canvas_read(v_index, &cs2);

	if (((vf->bitdepth & BITDEPTH_Y10)) &&
		(intfmt == GE2D_FORMAT_S16_YUV422) &&
		(get_cpu_type() < MESON_CPU_MAJOR_ID_TXL)) {
		pr_debug("vf->width = %d , vf->height = %d , vf->bitdepth = %d\n",
		vf->width, vf->height, vf->bitdepth);
		do_gettimeofday(&start);
		psrc = phys_to_virt(cs0.addr);
		w_align = ((vf->width + 32 - 1) & ~(32 - 1));
		h_align = ((vf->height + 32 - 1) & ~(32 - 1));
		temp_cma_buf_size =
		(int)((w_align * h_align * 2)/(1024 * 1024)) + 1;
		pr_debug("phybufaddr_8bit buffer size = %d\n",
			temp_cma_buf_size);
		phybufaddr_8bit = codec_mm_alloc_for_dma(CMA_NAME,
			temp_cma_buf_size * SZ_1M / PAGE_SIZE,
			4 + PAGE_SHIFT, CODEC_MM_FLAGS_CPU);
		if (!phybufaddr_8bit)
			pr_err("failed to alloc phybufaddr_8bit\n");

		canvas_config(temp_canvas_idx, phybufaddr_8bit,
			w_align * 2, h_align,
			CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		canvas_read(temp_y_index, &temp_cs0);
		canvas_read(temp_u_index, &temp_cs1);
		canvas_read(temp_v_index, &temp_cs2);

		pdst = phys_to_virt(temp_cs0.addr);

		pr_debug("height_after_di = %d" , height_after_di);
		line_start = psrc;
		for (i = 0; i < height_after_di; i++) {
			for (read_size = 0; read_size < w_align*3;
				read_size += 48) {
				/* swap 64bit */
				memcpy(buf1, line_start+read_size, 16);
				memcpy(tmp_buf,	buf1,		8);
				memcpy(buf1,	buf1+8,		8);
				memcpy(buf1+8,  tmp_buf,	8);
				memcpy(buf_in+32, buf1, 16);

				memcpy(buf2, line_start+read_size+16, 16);
				memcpy(tmp_buf,	buf2,		8);
				memcpy(buf2,	buf2+8,		8);
				memcpy(buf2+8,  tmp_buf,	8);
				memcpy(buf_in+16, buf2, 16);

				memcpy(buf3, line_start+read_size+32, 16);
				memcpy(tmp_buf,	buf3,		8);
				memcpy(buf3,	buf3+8,		8);
				memcpy(buf3+8,  tmp_buf,	8);
				memcpy(buf_in, buf3, 16);

				in_cursor = 47;
				out_cursor = 0;

				for (out_cursor = 0; out_cursor <= 30;
					out_cursor += 2, in_cursor -= 3) {
					buf_out[out_cursor] =
					(buf_in[in_cursor-1] << 4) |
					(buf_in[in_cursor-2] >> 4);
					buf_out[out_cursor+1] =
						buf_in[in_cursor];
				}
				for (out_cursor = 0; out_cursor <= 24;
					out_cursor += 8) {
					/* y1 y4 */
					tmp_char1 = buf_out[1+out_cursor];
					buf_out[1+out_cursor] =
						buf_out[7+out_cursor];
					buf_out[7+out_cursor] = tmp_char1;

					/* y2 y3 */
					tmp_char1 = buf_out[3+out_cursor];
					buf_out[3+out_cursor] =
						buf_out[5+out_cursor];
					buf_out[5+out_cursor] = tmp_char1;

					/* u1 u2 */
					tmp_char1 = buf_out[out_cursor];
					buf_out[out_cursor] =
						buf_out[2+out_cursor];
					buf_out[2+out_cursor] = tmp_char1;

					/* v1 v2 */
					tmp_char1 = buf_out[4+out_cursor];
					buf_out[4+out_cursor] =
						buf_out[6+out_cursor];
					buf_out[6+out_cursor] = tmp_char1;
				}

				memcpy(pdst+counter*32, buf_out, 32);
				counter += 1;
			}
			line_start += cs0.width;
		}

		codec_mm_dma_flush(pdst,
			temp_cma_buf_size * SZ_1M, DMA_TO_DEVICE);

		counter = 0;
		do_gettimeofday(&end);
		time_use = (end.tv_sec - start.tv_sec) * 1000 +
		(end.tv_usec - start.tv_usec) / 1000;
		pr_debug("10to8 conversion cost time: %ldms\n", time_use);
	}

	pr_debug("y_index=[0x%x]  u_index=[0x%x] cur_index:%x\n", y_index,
			u_index, cur_index);

	if (((vf->bitdepth & BITDEPTH_Y10)) &&
		(intfmt == GE2D_FORMAT_S16_YUV422) &&
		(get_cpu_type() < MESON_CPU_MAJOR_ID_TXL)) {
		ge2d_config.src_planes[0].addr = temp_cs0.addr;
		ge2d_config.src_planes[0].w = temp_cs0.width;
		ge2d_config.src_planes[0].h = temp_cs0.height;
		ge2d_config.src_planes[1].addr = temp_cs1.addr;
		ge2d_config.src_planes[1].w = temp_cs1.width;
		ge2d_config.src_planes[1].h = temp_cs1.height;
		ge2d_config.src_planes[2].addr = temp_cs2.addr;
		ge2d_config.src_planes[2].w = temp_cs2.width;
		ge2d_config.src_planes[2].h = temp_cs2.height;
		pr_debug("w=%d-height=%d\n", temp_cs0.width, temp_cs0.height);
		pr_debug("cs0.width=%d, cs0.height=%d\n", cs0.width, cs0.height);
	} else {
		ge2d_config.src_planes[0].addr = cs0.addr;
		ge2d_config.src_planes[0].w = cs0.width;
		ge2d_config.src_planes[0].h = cs0.height;
		ge2d_config.src_planes[1].addr = cs1.addr;
		ge2d_config.src_planes[1].w = cs1.width;
		ge2d_config.src_planes[1].h = cs1.height;
		ge2d_config.src_planes[2].addr = cs2.addr;
		ge2d_config.src_planes[2].w = cs2.width;
		ge2d_config.src_planes[2].h = cs2.height;
		pr_debug("w=%d-height=%d cur_index:%x\n",
			cs0.width, cs0.height, cur_index);
	}

	ge2d_config.src_key.key_enable = 0;
	ge2d_config.src_key.key_mask = 0;
	ge2d_config.src_key.key_mode = 0;
	ge2d_config.src_key.key_color = 0;

	if (((vf->bitdepth & BITDEPTH_Y10)) &&
		(intfmt == GE2D_FORMAT_S16_YUV422) &&
		(get_cpu_type() < MESON_CPU_MAJOR_ID_TXL))
		ge2d_config.src_para.canvas_index = temp_canvas_idx;
	else
		ge2d_config.src_para.canvas_index = cur_index;

	ge2d_config.src_para.mem_type = CANVAS_TYPE_INVALID;
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL) {
		if (intfmt == GE2D_FORMAT_S16_YUV422) {
			if ((vf->bitdepth & BITDEPTH_Y10) &&
				(vf->bitdepth & FULL_PACK_422_MODE)) {
				pr_debug("format is yuv422 10bit .\n");
				ge2d_config.src_para.format =
					GE2D_FORMAT_S16_10BIT_YUV422;
			} else if (vf->bitdepth & BITDEPTH_Y10) {
				pr_debug("format is yuv422 12bit .\n");
				ge2d_config.src_para.format =
					GE2D_FORMAT_S16_12BIT_YUV422;
			} else {
				ge2d_config.src_para.format = intfmt;
			}
		} else if (intfmt == GE2D_FORMAT_S24_YUV444) {
			if (vf->bitdepth & BITDEPTH_Y10) {
				pr_debug("format is yuv444 10bit .\n");
				ge2d_config.src_para.format =
					GE2D_FORMAT_S24_10BIT_YUV444;
			} else {
				ge2d_config.src_para.format = intfmt;
			}
		} else {
			ge2d_config.src_para.format = intfmt;
		}
	} else {
		ge2d_config.src_para.format = intfmt;
	}
	ge2d_config.src_para.fill_color_en = 0;
	ge2d_config.src_para.fill_mode = 0;
	ge2d_config.src_para.x_rev = 0;
	ge2d_config.src_para.y_rev = 0;
	ge2d_config.src_para.color = 0;
	ge2d_config.src_para.left = input_x;
	ge2d_config.src_para.top = input_y;
	ge2d_config.src_para.width = input_width;
	ge2d_config.src_para.height = input_height;

	canvas_read(canvas_idx, &cd);
	pr_debug("cd.addr:%x\n", (unsigned int)cd.addr);
	ge2d_config.dst_planes[0].addr = cd.addr;
	ge2d_config.dst_planes[0].w = cd.width;
	ge2d_config.dst_planes[0].h = cd.height;

	ge2d_config.dst_para.canvas_index = canvas_idx;
	ge2d_config.dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config.dst_para.format = outfmt;
	ge2d_config.dst_para.fill_color_en = 0;
	ge2d_config.dst_para.fill_mode = 0;
	ge2d_config.dst_para.x_rev = 0;
	ge2d_config.dst_para.y_rev = 0;
	ge2d_config.dst_xy_swap = 0;
	ge2d_config.dst_para.color = 0;
	ge2d_config.dst_para.top = 0;
	ge2d_config.dst_para.left = 0;
	ge2d_config.dst_para.width = w;
	ge2d_config.dst_para.height = h;

	if (ge2d_context_config_ex(ge2d_amvideocap_context, &ge2d_config) < 0) {
		pr_err("++ge2d configuration error.\n");
		return -1;
	}

	stretchblt_noalpha_noblk(ge2d_amvideocap_context,
					   0,
					   0,
					   ge2d_config.src_para.width,
					   ge2d_config.src_para.height,
					   0,
					   0,
					   ge2d_config.dst_para.width,
					   ge2d_config.dst_para.height);
	if (canvas_idx)
		canvas_pool_map_free_canvas(canvas_idx);

	if (((vf->bitdepth & BITDEPTH_Y10)) &&
		(intfmt == GE2D_FORMAT_S16_YUV422) &&
		(get_cpu_type() < MESON_CPU_MAJOR_ID_TXL)) {
		if (phybufaddr_8bit) {
			ret = codec_mm_free_for_dma(CMA_NAME, phybufaddr_8bit);
			if (ret != 0)
				pr_err("phybufaddr_8bit cma buffer free failed .\n");
		}
	if (temp_canvas_idx)
		canvas_pool_map_free_canvas(temp_canvas_idx);
	}

	return 0;
}

static int amvideocap_capture_one_frame_l(
		struct amvideocap_private *priv,
		int curindex, int w, int h,
		struct vframe_s *vf, int outge2dfmt)
{
	int ret;
	/* TODO: mod gate */
	/* switch_mod_gate_by_name("ge2d", 1); */
	ret = amvideocap_YUV_to_RGB(priv, curindex, w, h, vf, outge2dfmt);
	/* TODO: mod gate */
	/* switch_mod_gate_by_name("ge2d", 0); */
	return ret;
}

static int amvideocap_capture_one_frame(
	struct amvideocap_private *priv,
	struct vframe_s *vfput, int index)
{
	int w, h, ge2dfmt;
	int curindex;
	struct vframe_s *vf = vfput;
	int ret = 0;
	pr_debug("%s:start vf=%p,index=%x\n", __func__, vf, index);
	if (!vf)
		ret = amvideocap_capture_get_frame(priv, &vf, &curindex);
	else
		curindex = index;
	if (ret < 0 || !vf)
		return -EAGAIN;
	pr_debug("%s: get vf type=%x\n", __func__, vf->type);

#define CHECK_AND_SETVAL(want, def) ((want) > 0 ? (want) : (def))
	ge2dfmt = CHECK_AND_SETVAL(priv->want.fmt, vf->type);
	w = CHECK_AND_SETVAL(priv->want.width, vf->width);
	h = CHECK_AND_SETVAL(priv->want.height, vf->height);
#undef	CHECK_AND_SETVAL

	w = (w < CAP_WIDTH_MAX) ? w : CAP_WIDTH_MAX;
	h = (h < CAP_HEIGHT_MAX) ? h : CAP_HEIGHT_MAX;

	ret = amvideocap_capture_one_frame_l(priv, curindex, w, h, vf,
						priv->want.fmt);
	amvideocap_capture_put_frame(priv, vf);

	if (!ret) {
		pr_debug("%s: capture ok priv->want.fmt=%d\n", __func__,
				priv->want.fmt);
		priv->state = AMVIDEOCAP_STATE_FINISHED_CAPTURE;
		priv->src.width = vf->width;
		priv->src.height = vf->height;
		priv->out.timestamp_ms = vf->pts * 1000 / 90;
		priv->out.width = w;
		priv->out.height = h;
		priv->out.fmt = priv->want.fmt;
		priv->out.width_aligned = priv->out.width;
		priv->out.byte_per_pix = amvideocap_format_to_byte4pix(
			priv->out.fmt);	/* RGBn */
	} else
		priv->state = AMVIDEOCAP_STATE_ERROR;
	pr_debug("amvideocap_capture_one_frame priv->state=%d\n", priv->state);
	return ret;
}

static int amvideocap_capture_one_frame_wait(
		struct amvideocap_private *priv,
		int waitms)
{
	unsigned long timeout = jiffies + waitms * HZ / 1000;
	int ret = 0;
	struct amvideocap_req_data reqdata;
	struct amvideocap_req req;
	priv->sended_end_frame_cap_req = -EAGAIN;
	priv->state = AMVIDEOCAP_STATE_ON_CAPTURE;
	do {
		if (ret == -EAGAIN)
			usleep_range(500, 1000);
		if (priv->want.at_flags == CAP_FLAG_AT_END) {
			if (priv->sended_end_frame_cap_req == -EAGAIN) {
				reqdata.privdata = priv;
				req.callback =
					amvideocap_capture_one_frame;
				req.data = (unsigned long)&reqdata;
				req.at_flags = priv->want.at_flags;
				req.timestamp_ms = priv->want.timestamp_ms;
				priv->sended_end_frame_cap_req =
					ext_register_end_frame_callback(&req);
				ret = -EAGAIN;
			} else
				if (priv->sended_end_frame_cap_req == -ENODATA ||
							priv->state == AMVIDEOCAP_STATE_ERROR) {
				ret = -ENODATA;
			} else
				if (priv->state == AMVIDEOCAP_STATE_FINISHED_CAPTURE)
					ret = 0;
		} else {
			ret = amvideocap_capture_one_frame(priv, NULL, 0);
			pr_debug("amvideocap_capture_one_frame_wait ret=%d\n",
					ret);
		}
	} while (ret == -EAGAIN && time_before(jiffies, timeout));
	ext_register_end_frame_callback(NULL);	/*del req */
	return ret;
}

static long amvideocap_ioctl(struct file *file,
	unsigned int cmd, ulong arg)
{
	int ret = 0;
	struct amvideocap_private *priv = file->private_data;
	switch (cmd) {
	case AMVIDEOCAP_IOW_SET_WANTFRAME_FORMAT: {
		priv->want.fmt = arg;
		break;
	}
	case AMVIDEOCAP_IOW_SET_WANTFRAME_WIDTH: {
		priv->want.width = arg;
		break;
	}
	case AMVIDEOCAP_IOW_SET_WANTFRAME_HEIGHT: {
		priv->want.height = arg;
		break;
	}
	case AMVIDEOCAP_IOW_SET_WANTFRAME_TIMESTAMP_MS: {
		priv->want.timestamp_ms = arg;
		break;
	}
	case AMVIDEOCAP_IOW_SET_WANTFRAME_AT_FLAGS: {
		priv->want.at_flags = arg;
		break;
	}
	case AMVIDEOCAP_IOR_GET_FRAME_FORMAT: {
		if (copy_to_user
			((void *)arg, (void *)&priv->out.fmt,
			 sizeof(priv->out.fmt))) {
			ret = -EFAULT;
			break;
		}
		break;
	}
	case AMVIDEOCAP_IOR_GET_FRAME_WIDTH: {
		if (copy_to_user
			((void *)arg, (void *)&priv->out.width,
			 sizeof(priv->out.width))) {
			ret = -EFAULT;
			break;
		}
		break;
	}
	case AMVIDEOCAP_IOR_GET_FRAME_HEIGHT: {
		if (copy_to_user
			((void *)arg, (void *)&priv->out.height,
			 sizeof(priv->out.height))) {
			ret = -EFAULT;
			break;
		}
		break;
	}
	case AMVIDEOCAP_IOR_GET_FRAME_TIMESTAMP_MS: {
		if (copy_to_user
			((void *)arg, (void *)&priv->out.timestamp_ms,
			 sizeof(priv->out.timestamp_ms))) {
			ret = -EFAULT;
			break;
		}
		break;
	}
	case AMVIDEOCAP_IOR_GET_SRCFRAME_FORMAT: {
		if (copy_to_user
			((void *)arg, (void *)&priv->src.fmt,
			 sizeof(priv->src.fmt))) {
			ret = -EFAULT;
			break;
		}
		break;
	}
	case AMVIDEOCAP_IOR_GET_SRCFRAME_WIDTH: {
		if (copy_to_user
			((void *)arg, (void *)&priv->src.width,
			 sizeof(priv->src.width))) {
			ret = -EFAULT;
			break;
		}
		break;
	}
	case AMVIDEOCAP_IOR_GET_SRCFRAME_HEIGHT: {
		if (copy_to_user
			((void *)arg, (void *)&priv->src.height,
			 sizeof(priv->src.height))) {
			ret = -EFAULT;
			break;
		}
		break;
	}
	case AMVIDEOCAP_IOR_GET_STATE: {
		if (copy_to_user
			((void *)arg, (void *)&priv->state,
			 sizeof(priv->state))) {
			ret = -EFAULT;
			break;
		}
		break;
	}
	case AMVIDEOCAP_IOW_SET_WANTFRAME_WAIT_MAX_MS: {
		priv->wait_max_ms = arg;
		break;
	}
	case AMVIDEOCAP_IOW_SET_START_CAPTURE: {
		ret = amvideocap_capture_one_frame_wait(priv, arg);
		break;
	}
	case AMVIDEOCAP_IOW_SET_CANCEL_CAPTURE: {
		if (priv->sended_end_frame_cap_req) {
			ext_register_end_frame_callback(NULL);	/*del req */
			priv->sended_end_frame_cap_req = 0;
			priv->state = AMVIDEOCAP_STATE_INIT;
		}
		break;
	}
	case AMVIDEOCAP_IOR_SET_SRC_X: {
		priv->src_rect.x = arg;
		break;
	}
	case AMVIDEOCAP_IOR_SET_SRC_Y: {
		priv->src_rect.y = arg;
		break;
	}
	case AMVIDEOCAP_IOR_SET_SRC_WIDTH: {
		priv->src_rect.width = arg;
		break;
	}
	case AMVIDEOCAP_IOR_SET_SRC_HEIGHT: {
		priv->src_rect.height = arg;
		break;
	}
	default:
		pr_err("unkonw cmd=%x\n", cmd);
		ret = -1;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long amvideocap_compat_ioctl(struct file *file,
				unsigned int cmd, ulong arg)
{
	return amvideocap_ioctl(file, cmd, (ulong)compat_ptr(arg));
}
#endif
static int amvideocap_mmap(struct file *file,
	struct vm_area_struct *vma)
{
	struct amvideocap_private *priv = file->private_data;
	unsigned long off = vma->vm_pgoff << PAGE_SHIFT;
	unsigned vm_size = vma->vm_end - vma->vm_start;
	if (!priv->phyaddr)
		return -EIO;

	if (vm_size == 0)
		return -EAGAIN;
	/* pr_debug("mmap:%x\n",vm_size); */
	off += priv->phyaddr;

	vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP | VM_IO;
	if (remap_pfn_range(vma, vma->vm_start, off >> PAGE_SHIFT,
				vma->vm_end - vma->vm_start,
				vma->vm_page_prot)) {
		pr_err("set_cached: failed remap_pfn_range\n");
		return -EAGAIN;
	}
	pr_debug("amvideocap_mmap ok\n");
	return 0;
}

static ssize_t amvideocap_read(struct file *file, char __user *buf,
				   size_t count, loff_t *ppos)
{
	struct amvideocap_private *priv = file->private_data;
	int waitdelay;
	int ret = 0;
	int copied;
	loff_t pos;
	pos = *ppos;
	if (priv->wait_max_ms > 0)
		waitdelay = priv->wait_max_ms;
	else {
		if (priv->want.at_flags == CAP_FLAG_AT_END)	/*need end */
			waitdelay = file->f_flags & O_NONBLOCK ? HZ : HZ * 100;
		else
			waitdelay =
				file->f_flags & O_NONBLOCK ?
				HZ / 100 : HZ * 10;
	}
	if (!pos) {		/*trigger a new capture, */
		pr_debug("start amvideocap_read waitdelay=%d\n", waitdelay);
		ret = amvideocap_capture_one_frame_wait(priv, waitdelay);
		pr_debug("amvideocap_read=%d,priv->state=%d,priv->vaddr=%p\n",
				ret, priv->state, priv->vaddr);
		if ((ret == 0)
			&& (priv->state == AMVIDEOCAP_STATE_FINISHED_CAPTURE)
			&& (priv->vaddr != NULL)) {
			int size = min((int)count, (priv->out.byte_per_pix *
						priv->out.width_aligned *
						priv->out.height));
			pr_debug
			("priv->out_width=%d priv->out_height=%d",
			 priv->out.width, priv->out.height);
			pr_debug
			(" priv->outfmt_byteppix=%d, size=%d\n",
			 priv->out.byte_per_pix, size);
#ifdef CONFIG_CMA
			codec_mm_dma_flush(priv->vaddr,
					cma_max_size * SZ_1M, DMA_FROM_DEVICE);
#endif

			copied = copy_to_user(buf, priv->vaddr, size);
			if (copied) {
				pr_err
				("amvideocap_read %d copy_to_user failed\n",
				 size);
			}
			ret = size;
		}
	} else {
		/*read from old capture. */
		if (priv->state != AMVIDEOCAP_STATE_FINISHED_CAPTURE
			|| priv->vaddr == NULL) {
			ret = 0;	/*end. */
		} else {
			int maxsize =
				priv->out.byte_per_pix *
				priv->out.width_aligned * priv->out.height;
			if (pos < maxsize) {
				int rsize =
					min((int)count, (maxsize - (int)pos));
				copied =
					copy_to_user(buf, priv->vaddr + pos,
					rsize);
				if (copied) {
					pr_err
					("%s %d copy_to_user failed\n",
					 __func__, rsize);
				}
				ret = rsize;
			} else {
				ret = 0;	/*end. */
			}
		}
	}
	if (ret > 0) {
		pos += ret;
		*ppos = pos;
	}
	return ret;
}

static const struct file_operations amvideocap_fops = {
	.owner = THIS_MODULE,
	.open = amvideocap_open,
	.read = amvideocap_read,
	.mmap = amvideocap_mmap,
	.release = amvideocap_release,
	.unlocked_ioctl = amvideocap_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amvideocap_compat_ioctl,
#endif
	/* /  .poll     = amvideocap_poll, */
};

static ssize_t show_amvideocap_config(struct class *class,
					  struct class_attribute *attr,
					  char *buf)
{
	char *pbuf = buf;
	ssize_t size = 0;
	pbuf += sprintf(pbuf, "at_flags:%d\n", getgctrl()->want.at_flags);
	pbuf += sprintf(pbuf, "timestampms:%lld\n",
					getgctrl()->want.timestamp_ms);
	pbuf += sprintf(pbuf, "width:%d\n", getgctrl()->want.width);
	pbuf += sprintf(pbuf, "height:%d\n", getgctrl()->want.height);
	pbuf += sprintf(pbuf, "format:%d\n", getgctrl()->want.fmt);
	pbuf += sprintf(pbuf, "waitmaxms:%lld\n", getgctrl()->wait_max_ms);
	size = pbuf - buf;
	return size;
}

static ssize_t store_amvideocap_config(struct class *class,
						struct class_attribute *attr,
						const char *buf, size_t size)
{
	int ret, val;
	const char *pbuf = buf;
	for (; pbuf && pbuf[0] != '\0';) {
#ifdef GETVAL
#undef GETVAL
#endif
#define GETVAL(tag, v) do { \
		val = 0;\
		ret = sscanf(pbuf, tag ":%d", &val); \
		if (ret == 1) { \
			v = val; \
			pbuf += strlen(tag); \
			goto tonext; \
		}; \
	} while (0)
		GETVAL("timestamp", getgctrl()->want.timestamp_ms);
		GETVAL("width", getgctrl()->want.width);
		GETVAL("height", getgctrl()->want.height);
		GETVAL("format", getgctrl()->want.fmt);
		GETVAL("waitmaxms", getgctrl()->wait_max_ms);
		GETVAL("at_flags", getgctrl()->want.at_flags);
#undef GETVAL
		pbuf++;
tonext:
		while (pbuf[0] != ';' && pbuf[0] != '\0')
			pbuf++;
		if (pbuf[0] == ';')
			pbuf++;
	}
	return size;
}

static struct class_attribute amvideocap_class_attrs[] = {
	__ATTR(config, S_IRUGO | S_IWUSR | S_IWGRP, show_amvideocap_config,
	store_amvideocap_config),
	__ATTR_NULL
};

static struct class amvideocap_class = {
		.name = MODULE_NAME,
		.class_attrs = amvideocap_class_attrs,
	};

s32 amvideocap_register_memory(unsigned char *phybufaddr,
					int phybufsize)
{
	pr_debug("amvideocap_register_memory %p %d\n", phybufaddr, phybufsize);
	getgctrl()->phyaddr = (unsigned long)phybufaddr;
	getgctrl()->size = (unsigned long)phybufsize;
	getgctrl()->vaddr = 0;
	return 0;
}

s32 amvideocap_dev_register(unsigned char *phybufaddr, int phybufsize)
{
	s32 r = 0;
	pr_debug("amvideocap_dev_register\n");

	gLOCKINIT();
	r = register_chrdev(0, DEVICE_NAME, &amvideocap_fops);
	if (r < 0) {
		pr_err("Can't register major for amvideocap device\n");
		return r;
	}
	getgctrl()->major = r;
	r = class_register(&amvideocap_class);
	if (r) {
		pr_err("amvideocap class create fail.\n");
		goto err1;
	}
	getgctrl()->class = &amvideocap_class;
	getgctrl()->dev = device_create(getgctrl()->class,
					NULL, MKDEV(getgctrl()->major, 0),
					NULL, DEVICE_NAME "0");
	if (getgctrl()->dev == NULL) {
		pr_err("amvideocap device_create fail.\n");
		r = -EEXIST;
		goto err2;
	}
	if (phybufaddr != NULL) {
		getgctrl()->phyaddr = (unsigned long)phybufaddr;
		getgctrl()->size = (unsigned long)phybufsize;
	}
	getgctrl()->wait_max_ms = 0;
	getgctrl()->want.fmt = GE2D_FORMAT_S24_RGB;
	getgctrl()->want.width = 0;
	getgctrl()->want.height = 0;
	getgctrl()->want.timestamp_ms = 0;
	/*get last frame */
	getgctrl()->want.at_flags = CAP_FLAG_AT_CURRENT;
	return 0;
err2:
	class_unregister(&amvideocap_class);
err1:
	unregister_chrdev(getgctrl()->major, DEVICE_NAME);
	return r;
}

s32 amvideocap_dev_unregister(void)
{
	device_destroy(getgctrl()->class, MKDEV(getgctrl()->major, 0));
	class_unregister(getgctrl()->class);
	unregister_chrdev(getgctrl()->major, DEVICE_NAME);

	return 0;
}

/*******************************************************************
 *
 * interface for Linux driver
 *
 * ******************************************************************/

static struct resource memobj;
/* for driver. */
static int amvideocap_probe(struct platform_device *pdev)
{
	unsigned int buf_size;
	struct resource *mem;

#ifdef CONFIG_CMA
	char buf[32];
	u32 value;
	int ret;
#endif

	pr_err("amvideocap_probe,%s\n", pdev->dev.of_node->name);

#ifdef CONFIG_CMA
	snprintf(buf, sizeof(buf), "max_size");
	ret = of_property_read_u32(pdev->dev.of_node, buf, &value);
	if (ret < 0) {
		pr_err("cma size undefined.\n");
		use_cma = 0;
	} else {
		pr_err("use cma buf.\n");
		mem = &memobj;
		mem->start = 0;
		buf_size = 0;
		cma_max_size = value;
		amvideocap_pdev = pdev;
		use_cma = 1;
	}
#endif

	amvideocap_dev_register((unsigned char *)mem->start, buf_size);
	return 0;
}

static int amvideocap_remove(struct platform_device *plat_dev)
{
	/* struct rtc_device *rtc = platform_get_drvdata(plat_dev); */
	/* rtc_device_unregister(rtc); */
	/* device_remove_file(&plat_dev->dev, &dev_attr_irq); */
	amvideocap_dev_unregister();
	return 0;
}


static const struct of_device_id amlogic_amvideocap_dt_match[] = {
	{
		.compatible = "amlogic, amvideocap",
	},
	{},
};

/* general interface for a linux driver .*/
struct platform_driver amvideocap_drv = {
	.probe = amvideocap_probe,
	.remove = amvideocap_remove,
	.driver = {
		.name = "amvideocap",
		.of_match_table = amlogic_amvideocap_dt_match,
	}
};

static int __init amvideocap_init_module(void)
{

	pr_debug("amvideocap_init_module\n");
	if (ge2d_amvideocap_context == NULL)
		ge2d_amvideocap_context = create_ge2d_work_queue();
	if ((platform_driver_register(&amvideocap_drv))) {
		pr_err("failed to register amstream module\n");
		return -ENODEV;
	}
	return 0;

}

static void __exit amvideocap_remove_module(void)
{
	platform_driver_unregister(&amvideocap_drv);
	if (ge2d_amvideocap_context) {
		destroy_ge2d_work_queue(ge2d_amvideocap_context);
		ge2d_amvideocap_context = NULL;
	}
	pr_debug("amvideocap module removed.\n");
}

module_init(amvideocap_init_module);
module_exit(amvideocap_remove_module);

MODULE_DESCRIPTION("AMLOGIC  amvideocap driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wang Jian <jian.wang@amlogic.com>");
