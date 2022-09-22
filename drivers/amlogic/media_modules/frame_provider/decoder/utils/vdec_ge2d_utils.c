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
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include "vdec.h"
#include "vdec_ge2d_utils.h"
#include "../../../common/chips/decoder_cpu_ver_info.h"

static u32 vdec_ge2d_debug = 0;

#ifndef  CONFIG_AMLOGIC_MEDIA_GE2D
inline struct ge2d_context_s *create_ge2d_work_queue(void) { return NULL; }
inline int destroy_ge2d_work_queue(struct ge2d_context_s *ge2d_work_queue) { return -1; }
#endif

enum videocom_source_type {
	DECODER_8BIT_NORMAL = 0,
	DECODER_8BIT_BOTTOM,
	DECODER_8BIT_TOP,
	DECODER_10BIT_NORMAL,
	DECODER_10BIT_BOTTOM,
	DECODER_10BIT_TOP
};

static void vdec_canvas_cache_free(struct vdec_canvas_cache *canche)
{
	int i = -1;

	for (i = 0; i < ARRAY_SIZE(canche->res); i++) {
		if (canche->res[i].cid > 0) {
			if (vdec_ge2d_debug)
				pr_info("canvas-free, name:%s, canvas id:%d\n",
					canche->res[i].name,
					canche->res[i].cid);

			canvas_pool_map_free_canvas(canche->res[i].cid);

			canche->res[i].cid = 0;
		}
	}
}

static void vdec_canvas_cache_put(struct vdec_ge2d *ge2d)
{
	struct vdec_canvas_cache *canche = &ge2d->canche;

	mutex_lock(&canche->lock);

	pr_info("canvas-put, ref:%d\n", canche->ref);

	canche->ref--;

	if (canche->ref == 0) {
		vdec_canvas_cache_free(canche);
	}

	mutex_unlock(&canche->lock);
}

static int vdec_canvas_cache_get(struct vdec_ge2d *ge2d, char *usr)
{
	struct vdec_canvas_cache *canche = &ge2d->canche;
	int i;

	mutex_lock(&canche->lock);

	canche->ref++;

	for (i = 0; i < ARRAY_SIZE(canche->res); i++) {
		if (canche->res[i].cid <= 0) {
			snprintf(canche->res[i].name, 32, "%s-%d", usr, i);
			canche->res[i].cid =
				canvas_pool_map_alloc_canvas(canche->res[i].name);
		}

		if (vdec_ge2d_debug)
			pr_info("canvas-alloc, name:%s, canvas id:%d\n",
				canche->res[i].name,
				canche->res[i].cid);

		if (canche->res[i].cid <= 0) {
			pr_err("canvas-fail, name:%s, canvas id:%d.\n",
				canche->res[i].name,
				canche->res[i].cid);

			mutex_unlock(&canche->lock);
			goto err;
		}
	}

	if (vdec_ge2d_debug)
		pr_info("canvas-get, ref:%d\n", canche->ref);

	mutex_unlock(&canche->lock);
	return 0;
err:
	vdec_canvas_cache_put(ge2d);
	return -1;
}

static int vdec_canvas_cache_init(struct vdec_ge2d *ge2d)
{
	ge2d->canche.ref = 0;
	mutex_init(&ge2d->canche.lock);

	pr_info("canvas-init, ref:%d\n", ge2d->canche.ref);

	return 0;
}

static int get_source_type(struct vframe_s *vf)
{
	enum videocom_source_type ret;
	int interlace_mode;

	interlace_mode = vf->type & VIDTYPE_TYPEMASK;

	if ((vf->bitdepth & BITDEPTH_Y10)  &&
		(!(vf->type & VIDTYPE_COMPRESS)) &&
		(get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL)) {
		if (interlace_mode == VIDTYPE_INTERLACE_TOP)
			ret = DECODER_10BIT_TOP;
		else if (interlace_mode == VIDTYPE_INTERLACE_BOTTOM)
			ret = DECODER_10BIT_BOTTOM;
		else
			ret = DECODER_10BIT_NORMAL;
	} else {
		if (interlace_mode == VIDTYPE_INTERLACE_TOP)
			ret = DECODER_8BIT_TOP;
		else if (interlace_mode == VIDTYPE_INTERLACE_BOTTOM)
			ret = DECODER_8BIT_BOTTOM;
		else
			ret = DECODER_8BIT_NORMAL;
	}

	return ret;
}

static int get_input_format(struct vframe_s *vf)
{
	int format = GE2D_FORMAT_M24_YUV420;
	enum videocom_source_type soure_type;

	soure_type = get_source_type(vf);

	switch (soure_type) {
	case DECODER_8BIT_NORMAL:
		if (vf->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422;
		else if (vf->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21;
		else if (vf->type & VIDTYPE_VIU_NV12)
			format = GE2D_FORMAT_M24_NV12;
		else if (vf->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444;
		else
			format = GE2D_FORMAT_M24_YUV420;
		break;
	case DECODER_8BIT_BOTTOM:
		if (vf->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422
				| (GE2D_FORMAT_S16_YUV422B & (3 << 3));
		else if (vf->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21
				| (GE2D_FORMAT_M24_NV21B & (3 << 3));
		else if (vf->type & VIDTYPE_VIU_NV12)
			format = GE2D_FORMAT_M24_NV12
				| (GE2D_FORMAT_M24_NV12B & (3 << 3));
		else if (vf->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444
				| (GE2D_FORMAT_S24_YUV444B & (3 << 3));
		else
			format = GE2D_FORMAT_M24_YUV420
				| (GE2D_FMT_M24_YUV420B & (3 << 3));
		break;
	case DECODER_8BIT_TOP:
		if (vf->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422
				| (GE2D_FORMAT_S16_YUV422T & (3 << 3));
		else if (vf->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21
				| (GE2D_FORMAT_M24_NV21T & (3 << 3));
		else if (vf->type & VIDTYPE_VIU_NV12)
			format = GE2D_FORMAT_M24_NV12
				| (GE2D_FORMAT_M24_NV12T & (3 << 3));
		else if (vf->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444
				| (GE2D_FORMAT_S24_YUV444T & (3 << 3));
		else
			format = GE2D_FORMAT_M24_YUV420
				| (GE2D_FMT_M24_YUV420T & (3 << 3));
		break;
	case DECODER_10BIT_NORMAL:
		if (vf->type & VIDTYPE_VIU_422) {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422;
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422;
		}
		break;
	case DECODER_10BIT_BOTTOM:
		if (vf->type & VIDTYPE_VIU_422) {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422
					| (GE2D_FORMAT_S16_10BIT_YUV422B
					& (3 << 3));
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422
					| (GE2D_FORMAT_S16_12BIT_YUV422B
					& (3 << 3));
		}
		break;
	case DECODER_10BIT_TOP:
		if (vf->type & VIDTYPE_VIU_422) {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422
					| (GE2D_FORMAT_S16_10BIT_YUV422T
					& (3 << 3));
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422
					| (GE2D_FORMAT_S16_12BIT_YUV422T
					& (3 << 3));
		}
		break;
	default:
		format = GE2D_FORMAT_M24_YUV420;
	}
	return format;
}

int vdec_ge2d_init(struct vdec_ge2d** ge2d_handle, int mode)
{
	int ret;
	struct vdec_ge2d *ge2d;

	if (!ge2d_handle)
		return -EINVAL;

	ge2d = kzalloc(sizeof(*ge2d), GFP_KERNEL);
	if (!ge2d)
		return -ENOMEM;

	vdec_canvas_cache_init(ge2d);

	ge2d->work_mode = mode;
	if (!ge2d->work_mode) {
		ge2d->work_mode = GE2D_MODE_CONVERT_LE;
	}
	pr_info("vdec_ge2d_init work_mode:%d\n", ge2d->work_mode);

	ge2d->ge2d_context = create_ge2d_work_queue();
	if (!ge2d->ge2d_context) {
		pr_err("ge2d_create_instance fail\n");
		ret = -EINVAL;
		goto error;
	}

	if (vdec_canvas_cache_get(ge2d, "vdec-ge2d-dec") < 0) {
		pr_err("canvas pool alloc fail. src(%d, %d, %d) dst(%d, %d, %d).\n",
			ge2d->canche.res[0].cid,
			ge2d->canche.res[1].cid,
			ge2d->canche.res[2].cid,
			ge2d->canche.res[3].cid,
			ge2d->canche.res[4].cid,
			ge2d->canche.res[5].cid);
		ret = -ENOMEM;
		goto error1;
	}

	*ge2d_handle = ge2d;

	return 0;
error1:
	destroy_ge2d_work_queue(ge2d->ge2d_context);
error:
	kfree(ge2d);

	return ret;
}
EXPORT_SYMBOL(vdec_ge2d_init);

int vdec_ge2d_copy_data(struct vdec_ge2d *ge2d, struct vdec_ge2d_info *ge2d_info)
{
	struct config_para_ex_s ge2d_config;
	u32 src_fmt = 0, dst_fmt = 0;
	struct canvas_s cd;

	if (!ge2d)
		return -1;

	if (vdec_ge2d_debug)
		pr_info("vdec_ge2d_copy_data start\n");

	memset(&ge2d_config, 0, sizeof(ge2d_config));

	src_fmt = get_input_format(ge2d_info->dst_vf);
	if (ge2d_info->src_canvas0_config[0].endian == 7)
		src_fmt |= GE2D_BIG_ENDIAN;
	else
		src_fmt |= GE2D_LITTLE_ENDIAN;

	/* negotiate format of destination */
	dst_fmt = get_input_format(ge2d_info->dst_vf);
	if (ge2d->work_mode & GE2D_MODE_CONVERT_NV12)
		dst_fmt |= GE2D_FORMAT_M24_NV12;
	else if (ge2d->work_mode & GE2D_MODE_CONVERT_NV21)
		dst_fmt |= GE2D_FORMAT_M24_NV21;

	if (ge2d->work_mode & GE2D_MODE_CONVERT_LE)
		dst_fmt |= GE2D_LITTLE_ENDIAN;
	else
		dst_fmt |= GE2D_BIG_ENDIAN;

	if ((dst_fmt & GE2D_COLOR_MAP_MASK) == GE2D_COLOR_MAP_NV12) {
		ge2d_info->dst_vf->type |= VIDTYPE_VIU_NV12;
		ge2d_info->dst_vf->type &= ~VIDTYPE_VIU_NV21;
	} else if ((dst_fmt & GE2D_COLOR_MAP_MASK) == GE2D_COLOR_MAP_NV21) {
		ge2d_info->dst_vf->type |= VIDTYPE_VIU_NV21;
		ge2d_info->dst_vf->type &= ~VIDTYPE_VIU_NV12;
	}
	if ((dst_fmt & GE2D_ENDIAN_MASK) == GE2D_LITTLE_ENDIAN) {
		ge2d_info->dst_vf->canvas0_config[0].endian = 0;
		ge2d_info->dst_vf->canvas0_config[1].endian = 0;
		ge2d_info->dst_vf->canvas0_config[2].endian = 0;
	} else if ((dst_fmt & GE2D_ENDIAN_MASK) == GE2D_BIG_ENDIAN){
		ge2d_info->dst_vf->canvas0_config[0].endian = 7;
		ge2d_info->dst_vf->canvas0_config[1].endian = 7;
		ge2d_info->dst_vf->canvas0_config[2].endian = 7;
	}

	ge2d_info->dst_vf->mem_sec = ge2d_info->dst_vf->flag & VFRAME_FLAG_VIDEO_SECURE ? 1 : 0;

	mutex_lock(&ge2d->canche.lock);

	/* src canvas configure. */
	if ((ge2d_info->dst_vf->canvas0Addr == 0) ||
		(ge2d_info->dst_vf->canvas0Addr == (u32)-1)) {
		canvas_config_config(ge2d->canche.res[0].cid, &ge2d_info->src_canvas0_config[0]);
		canvas_config_config(ge2d->canche.res[1].cid, &ge2d_info->src_canvas0_config[1]);
		canvas_config_config(ge2d->canche.res[2].cid, &ge2d_info->src_canvas0_config[2]);
		ge2d_config.src_para.canvas_index =
			ge2d->canche.res[0].cid |
			ge2d->canche.res[1].cid << 8 |
			ge2d->canche.res[2].cid << 16;

		ge2d_config.src_planes[0].addr =
			ge2d_info->src_canvas0_config[0].phy_addr;
		ge2d_config.src_planes[0].w =
			ge2d_info->src_canvas0_config[0].width;
		ge2d_config.src_planes[0].h =
			ge2d_info->src_canvas0_config[0].height;
		ge2d_config.src_planes[1].addr =
			ge2d_info->src_canvas0_config[1].phy_addr;
		ge2d_config.src_planes[1].w =
			ge2d_info->src_canvas0_config[1].width;
		ge2d_config.src_planes[1].h =
			ge2d_info->src_canvas0_config[1].height;
		ge2d_config.src_planes[2].addr =
			ge2d_info->src_canvas0_config[2].phy_addr;
		ge2d_config.src_planes[2].w =
			ge2d_info->src_canvas0_config[2].width;
		ge2d_config.src_planes[2].h =
			ge2d_info->src_canvas0_config[2].height;
	} else {
		ge2d_config.src_para.canvas_index = ge2d_info->src_canvas0Addr;
	}
	ge2d_config.src_para.mem_type	= CANVAS_TYPE_INVALID;
	ge2d_config.src_para.format = src_fmt;
	ge2d_config.src_para.fill_color_en = 0;
	ge2d_config.src_para.fill_mode	= 0;
	ge2d_config.src_para.x_rev	= 0;
	ge2d_config.src_para.y_rev	= 0;
	ge2d_config.src_para.color	= 0xffffffff;
	ge2d_config.src_para.top	= 0;
	ge2d_config.src_para.left	= 0;
	ge2d_config.src_para.width	= ge2d_info->dst_vf->width;
	if (ge2d_info->dst_vf->type & VIDTYPE_INTERLACE)
		ge2d_config.src_para.height = ge2d_info->dst_vf->height >> 1;
	else
		ge2d_config.src_para.height = ge2d_info->dst_vf->height;

	/* dst canvas configure. */
	canvas_config_config(ge2d->canche.res[3].cid, &ge2d_info->dst_vf->canvas0_config[0]);
	if ((ge2d_config.src_para.format & 0xfffff) == GE2D_FORMAT_M24_YUV420) {
		ge2d_info->dst_vf->canvas0_config[1].width <<= 1;
	}
	canvas_config_config(ge2d->canche.res[4].cid, &ge2d_info->dst_vf->canvas0_config[1]);
	canvas_config_config(ge2d->canche.res[5].cid, &ge2d_info->dst_vf->canvas0_config[2]);
	ge2d_config.dst_para.canvas_index =
		ge2d->canche.res[3].cid |
		ge2d->canche.res[4].cid << 8;
	canvas_read(ge2d->canche.res[3].cid, &cd);
	ge2d_config.dst_planes[0].addr	= cd.addr;
	ge2d_config.dst_planes[0].w	= cd.width;
	ge2d_config.dst_planes[0].h	= cd.height;
	canvas_read(ge2d->canche.res[4].cid, &cd);
	ge2d_config.dst_planes[1].addr	= cd.addr;
	ge2d_config.dst_planes[1].w	= cd.width;
	ge2d_config.dst_planes[1].h	= cd.height;

	ge2d_config.dst_para.format	=  dst_fmt;
	ge2d_config.dst_para.width	= ge2d_info->dst_vf->width;
	ge2d_config.dst_para.height	= ge2d_info->dst_vf->height;
	ge2d_config.dst_para.mem_type	= CANVAS_TYPE_INVALID;
	ge2d_config.dst_para.fill_color_en = 0;
	ge2d_config.dst_para.fill_mode	= 0;
	ge2d_config.dst_para.x_rev	= 0;
	ge2d_config.dst_para.y_rev	= 0;
	ge2d_config.dst_para.color	= 0;
	ge2d_config.dst_para.top	= 0;
	ge2d_config.dst_para.left	= 0;

	/* other ge2d parameters configure. */
	ge2d_config.src_key.key_enable	= 0;
	ge2d_config.src_key.key_mask	= 0;
	ge2d_config.src_key.key_mode	= 0;
	ge2d_config.alu_const_color	= 0;
	ge2d_config.bitmask_en		= 0;
	ge2d_config.src1_gb_alpha	= 0;
	ge2d_config.dst_xy_swap		= 0;
	ge2d_config.src2_para.mem_type	= CANVAS_TYPE_INVALID;
	ge2d_config.mem_sec	= ge2d_info->dst_vf->flag & VFRAME_FLAG_VIDEO_SECURE ? 1 : 0;

	if (ge2d_context_config_ex(ge2d->ge2d_context, &ge2d_config) < 0) {
		pr_err("vdec_ge2d_context_config_ex error.\n");
		mutex_unlock(&ge2d->canche.lock);
		return -1;
	}

	if (!(ge2d_info->dst_vf->type & VIDTYPE_V4L_EOS)) {
		if (ge2d_info->dst_vf->type & VIDTYPE_INTERLACE) {
			stretchblt_noalpha(ge2d->ge2d_context,
				0, 0, ge2d_info->dst_vf->width, ge2d_info->dst_vf->height / 2,
				0, 0, ge2d_info->dst_vf->width, ge2d_info->dst_vf->height);
		} else {
			stretchblt_noalpha(ge2d->ge2d_context,
				0, 0, ge2d_info->dst_vf->width, ge2d_info->dst_vf->height,
				0, 0, ge2d_info->dst_vf->width, ge2d_info->dst_vf->height);
		}
	}
	mutex_unlock(&ge2d->canche.lock);
	if (vdec_ge2d_debug)
		pr_info("vdec_ge2d_copy_data done\n");

	return 0;
}
EXPORT_SYMBOL(vdec_ge2d_copy_data);

int vdec_ge2d_destroy(struct vdec_ge2d *ge2d)
{
	if (!ge2d)
		return -1;

	pr_info("vdec ge2d destroy begin\n");

	destroy_ge2d_work_queue(ge2d->ge2d_context);
	vdec_canvas_cache_put(ge2d);
	kfree(ge2d);

	pr_info("vdec ge2d destroy done\n");

	return 0;
}
EXPORT_SYMBOL(vdec_ge2d_destroy);

module_param(vdec_ge2d_debug, int, 0664);
MODULE_PARM_DESC(vdec_ge2d_debug, "\n vdec_ge2d_debug\n");
