// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#endif
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include "../../media/video_processor/video_composer/videodisplay.h"
#include "meson_vpu_pipeline.h"
#include "meson_crtc.h"
#include "meson_vpu_reg.h"
#include "meson_vpu_util.h"
#include "meson_drv.h"
#include "meson_plane.h"

static int video_axis_zoom = -1;
module_param(video_axis_zoom, int, 0664);
MODULE_PARM_DESC(video_axis_zoom, "video_axis_zoom");

static void
video_vfm_convert_to_vfminfo(struct meson_vpu_video_state *mvvs,
	struct video_display_frame_info_t *vf_info)
{
	vf_info->dmabuf = mvvs->dmabuf;
	vf_info->dst_x = mvvs->dst_x;
	vf_info->dst_y = mvvs->dst_y;
	vf_info->dst_w = mvvs->dst_w;
	vf_info->dst_h = mvvs->dst_h;
	vf_info->crop_x = mvvs->src_x;
	vf_info->crop_y = mvvs->src_y;
	vf_info->crop_w = mvvs->src_w;
	vf_info->crop_h = mvvs->src_h;
	vf_info->buffer_w = mvvs->fb_w;
	vf_info->buffer_h = mvvs->fb_h;
	vf_info->zorder = mvvs->zorder;

	MESON_DRM_BLOCK("dmabuf = %px, release_fence = %px\n",
					vf_info->dmabuf, vf_info->release_fence);
	MESON_DRM_BLOCK("vf-info crop:%u, %u, %u, %u, pic:%u, %u\n",
				vf_info->crop_x, vf_info->crop_y, vf_info->crop_w, vf_info->crop_h,
				vf_info->buffer_w, vf_info->buffer_h);
}

static u32 video_type_get(u32 pixel_format)
{
	u32 vframe_type = 0;

	switch (pixel_format) {
	case DRM_FORMAT_NV12:
		vframe_type = VIDTYPE_VIU_NV12 | VIDTYPE_VIU_FIELD |
				VIDTYPE_PROGRESSIVE;
		break;
	case DRM_FORMAT_NV21:
		vframe_type = VIDTYPE_VIU_NV21 | VIDTYPE_VIU_FIELD |
				VIDTYPE_PROGRESSIVE;
		break;
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
		vframe_type = VIDTYPE_VIU_422 | VIDTYPE_VIU_FIELD |
				VIDTYPE_VIU_SINGLE_PLANE;
		break;
	case DRM_FORMAT_VUY888:
		vframe_type = VIDTYPE_VIU_444 | VIDTYPE_VIU_FIELD |
				VIDTYPE_VIU_SINGLE_PLANE;
		break;
	default:
		DRM_INFO("no support pixel format:0x%x\n", pixel_format);
		break;
	}
	return vframe_type;
}

static int bind_video_fence_vframe(struct meson_vpu_video *video,
			struct dma_fence *fence, u32 index, struct vframe_s *vf)
{
	struct vf_node *cur_node;
	struct meson_video_plane_fence_info *info;

	cur_node = kzalloc(sizeof(*cur_node), GFP_KERNEL);
	if (!cur_node)
		return -ENOMEM;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		kfree(cur_node);
		return -ENOMEM;
	}
	info->fence = fence;
	info->vf = vf;
	atomic_set(&info->refcount, 1);
	cur_node->infos = info;

	list_add_tail(&cur_node->vfm_node, &video->vfm_node[index]);
	MESON_DRM_FENCE("%s, fence-%px, vf-%px\n", __func__, fence, vf);
	return 0;
}

static void video_fence_signal(struct meson_vpu_video *video,
			struct vframe_s *vf)
{
	struct vf_node *cur_node;
	struct meson_video_plane_fence_info *info_cur;
	struct list_head *p, *n;
	int i;

	for (i = 0; i < MESON_MAX_VIDEO; i++) {
		list_for_each_safe(p, n, &video->vfm_node[i]) {
			cur_node = list_entry(p, struct vf_node, vfm_node);
			info_cur = cur_node->infos;
			if (vf && info_cur->vf == vf) {
				atomic_dec(&info_cur->refcount);
				if (!atomic_read(&info_cur->refcount)) {
					MESON_DRM_FENCE("%s, fence-%px, vf-%px\n",
						__func__, info_cur->fence, vf);
					dma_fence_signal(info_cur->fence);
					dma_fence_put(info_cur->fence);
					info_cur->fence = NULL;
					info_cur->vf = NULL;
					atomic_set(&info_cur->refcount, 0);
					list_del_init(p);
					kfree(cur_node);
					return;
				}
			}
		}
	}
}

static void video_disable_fence(struct meson_vpu_video *video)
{
	struct vf_node *cur_node;
	struct meson_video_plane_fence_info *info_cur;
	struct list_head *p, *n;
	int i;

	for (i = 0; i < MESON_MAX_VIDEO; i++) {
		list_for_each_safe(p, n, &video->vfm_node[i]) {
			cur_node = list_entry(p, struct vf_node, vfm_node);
			info_cur = cur_node->infos;
			atomic_dec(&info_cur->refcount);
			if (!atomic_read(&info_cur->refcount)) {
				MESON_DRM_FENCE("%s, fence-%px, vf-%px\n",
				__func__, info_cur->fence, info_cur->vf);
				dma_fence_signal(info_cur->fence);
				dma_fence_put(info_cur->fence);
				info_cur->fence = NULL;
				info_cur->vf = NULL;
				atomic_set(&info_cur->refcount, 0);
				list_del_init(p);
				kfree(cur_node);
			}
		}
	}
}

/*background data R[32:47] G[16:31] B[0:15] alpha[48:63]->
 *dummy data Y[16:23] Cb[8:15] Cr[0:7]
 */
static void video_dummy_data_set(struct am_meson_crtc_state *crtc_state)
{
	int r, g, b, alpha, y, u, v;
	u32 vpp_index = 0;

	b = (crtc_state->crtc_bgcolor & 0xffff) / 256;
	g = ((crtc_state->crtc_bgcolor >> 16) & 0xffff) / 256;
	r = ((crtc_state->crtc_bgcolor >> 32) & 0xffff) / 256;
	alpha = ((crtc_state->crtc_bgcolor >> 48) & 0xffff) / 256;

	y = ((47 * r + 157 * g + 16 * b + 128) >> 8) + 16;
	u = ((-26 * r - 87 * g + 113 * b + 128) >> 8) + 128;
	v = ((112 * r - 102 * g - 10 * b + 128) >> 8) + 128;

	set_post_blend_dummy_data(vpp_index, 1 << 24 | y << 16 | u << 8 | v, alpha);
}

/* -----------------------------------------------------------------
 *           provider operations
 * -----------------------------------------------------------------
 */
static struct vframe_s *vp_vf_peek(void *op_arg)
{
	struct meson_vpu_video *video = (struct meson_vpu_video *)op_arg;
	struct vframe_s *vf = NULL;

	if (kfifo_peek(&video->ready_q, &vf))
		return vf;
	else
		return NULL;
}

static struct vframe_s *vp_vf_get(void *op_arg)
{
	struct meson_vpu_video *video = (struct meson_vpu_video *)op_arg;
	struct vframe_s *vf = NULL;

	if (kfifo_get(&video->ready_q, &vf)) {
		if (!vf)
			return NULL;
		return vf;
	} else {
		return NULL;
	}
}

static void vp_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct meson_vpu_video *video = (struct meson_vpu_video *)op_arg;

	if (!vf)
		return;

	kfifo_put(&video->free_q, vf);

	video_fence_signal(video, vf);
}

static int vp_event_cb(int type, void *data, void *private_data)
{
	if (type & VFRAME_EVENT_RECEIVER_PUT)
		;
	else if (type & VFRAME_EVENT_RECEIVER_GET)
		;
	else if (type & VFRAME_EVENT_RECEIVER_FRAME_WAIT)
		;
	return 0;
}

static int vp_vf_states(struct vframe_states *states, void *op_arg)
{
	struct meson_vpu_video *video = (struct meson_vpu_video *)op_arg;

	states->vf_pool_size = BUFFER_NUM;
	states->buf_recycle_num = 0;
	states->buf_free_num = kfifo_len(&video->free_q);
	states->buf_avail_num = kfifo_len(&video->ready_q);
	return 0;
}

static const struct vframe_operations_s vp_vf_ops = {
	.peek = vp_vf_peek,
	.get  = vp_vf_get,
	.put  = vp_vf_put,
	.event_cb = vp_event_cb,
	.vf_states = vp_vf_states,
};

static int video_check_state(struct meson_vpu_block *vblk,
			     struct meson_vpu_block_state *state,
		struct meson_vpu_pipeline_state *mvps)
{
	struct meson_vpu_video_layer_info *plane_info;
	struct meson_vpu_video *video = to_video_block(vblk);
	struct meson_vpu_video_state *mvvs = to_video_state(state);
	int i;

	if (state->checked)
		return 0;

	state->checked = true;

	if (!mvvs || mvvs->plane_index >= MESON_MAX_VIDEO) {
		DRM_INFO("mvvs is NULL!\n");
		return -1;
	}
	MESON_DRM_BLOCK("%s check_state called.\n", video->base.name);
	plane_info = &mvps->video_plane_info[vblk->index];
	mvvs->src_x = plane_info->src_x;
	mvvs->src_y = plane_info->src_y;
	mvvs->src_w = plane_info->src_w;
	mvvs->src_h = plane_info->src_h;
	mvvs->dst_x = plane_info->dst_x;
	mvvs->dst_y = plane_info->dst_y;
	mvvs->dst_w = plane_info->dst_w;
	mvvs->dst_h = plane_info->dst_h;
	mvvs->fb_w = plane_info->fb_w;
	mvvs->fb_h = plane_info->fb_h;
	mvvs->byte_stride = plane_info->byte_stride;
	mvvs->plane_index = plane_info->plane_index;
	mvvs->phy_addr[0] = plane_info->phy_addr[0];
	mvvs->phy_addr[1] = plane_info->phy_addr[1];
	mvvs->zorder = plane_info->zorder;
	mvvs->pixel_format = plane_info->pixel_format;
	mvvs->fb_size[0] = plane_info->fb_size[0];
	mvvs->fb_size[1] = plane_info->fb_size[1];
	mvvs->vf = plane_info->vf;
	mvvs->is_uvm = plane_info->is_uvm;
	video->vfm_mode = plane_info->vfm_mode;
	mvvs->dmabuf = plane_info->dmabuf;
	mvvs->crtc_index = plane_info->crtc_index;

	if (!video->vfm_mode && !video->video_path_reg) {
		kfifo_reset(&video->ready_q);
		kfifo_reset(&video->free_q);
		kfifo_reset(&video->display_q);
		for (i = 0; i < BUFFER_NUM; i++)
			kfifo_put(&video->free_q, &video->vframe[i]);
		vf_reg_provider(&video->vprov);
		vf_notify_receiver(video->base.name,
				   VFRAME_EVENT_PROVIDER_START, NULL);
		video->video_path_reg = 1;
	}
	return 0;
}

static void video_set_state(struct meson_vpu_block *vblk,
			    struct meson_vpu_block_state *state,
			    struct meson_vpu_block_state *old_state)
{
	int crtc_index;
	struct am_meson_crtc *amc;
	struct am_meson_crtc_state *meson_crtc_state;
	struct meson_vpu_video *video = to_video_block(vblk);
	struct meson_vpu_video_state *mvvs = to_video_state(state);
	struct video_display_frame_info_t vf_info;
	struct vframe_s *vf = NULL, *dec_vf = NULL, *vfp = NULL;
	u32 pixel_format, src_h, byte_stride, pic_w, pic_h, repeat_frame;
	u32 recal_src_w, recal_src_h;
	u64 phy_addr, phy_addr2 = 0;

	MESON_DRM_BLOCK("%s", __func__);

	if (!vblk) {
		DRM_DEBUG("set_state break for NULL.\n");
		return;
	}

	crtc_index = mvvs->crtc_index;
	amc = vblk->pipeline->priv->crtcs[crtc_index];
	meson_crtc_state = to_am_meson_crtc_state(amc->base.state);
	video_dummy_data_set(meson_crtc_state);

	repeat_frame = video->repeat_frame;
	src_h = mvvs->src_h;
	byte_stride = mvvs->byte_stride;
	phy_addr = mvvs->phy_addr[0];
	pixel_format = mvvs->pixel_format;
	MESON_DRM_BLOCK("%s %d-%d-%llx", __func__, src_h, pixel_format, phy_addr);

	if (mvvs->is_uvm && mvvs->vf) {
		dec_vf = mvvs->vf;
		vf = mvvs->vf;
		MESON_DRM_BLOCK("dec vf, %s, flag-%u, type-%u, %u, %u, %u, %u\n",
			  __func__, dec_vf->flag, dec_vf->type,
			  dec_vf->compWidth, dec_vf->compHeight,
			  dec_vf->width, dec_vf->height);
		if (vf->vf_ext && (vf->flag & VFRAME_FLAG_CONTAIN_POST_FRAME)) {
			vf = mvvs->vf->vf_ext;
			MESON_DRM_BLOCK("DI vf, %s, flag-%u, type-%u, %u,%u,%u,%u\n",
				  __func__, vf->flag, vf->type,
				  vf->compWidth, vf->compHeight,
				  vf->width, vf->height);
		}
		vf->axis[0] = mvvs->dst_x;
		vf->axis[1] = mvvs->dst_y;
		vf->axis[2] = mvvs->dst_x + mvvs->dst_w - 1;
		vf->axis[3] = mvvs->dst_y + mvvs->dst_h - 1;
		vf->crop[0] = mvvs->src_y;/*crop top*/
		vf->crop[1] = mvvs->src_x;/*crop left*/

		/*
		 *if video_axis_zoom = 1, means the video anix is
		 *set by westeros
		 */
		if (video_axis_zoom != -1) {
			if (dec_vf->type & VIDTYPE_COMPRESS) {
				pic_w = dec_vf->compWidth;
				pic_h = dec_vf->compHeight;
				recal_src_w = mvvs->src_w *
							pic_w / dec_vf->width;
				recal_src_h = mvvs->src_h *
							pic_h / dec_vf->height;
				vf->crop[0] = mvvs->src_y *
							pic_h / dec_vf->height;
				vf->crop[1] = mvvs->src_x *
							pic_w / dec_vf->width;
			} else {
				pic_w = dec_vf->width;
				pic_h = dec_vf->height;
				recal_src_w = mvvs->src_w;
				recal_src_h = mvvs->src_h;
			}
		} else {
			recal_src_w = mvvs->src_w;
			recal_src_h = mvvs->src_h;
			if (dec_vf->type & VIDTYPE_COMPRESS) {
				pic_w = dec_vf->compWidth;
				pic_h = dec_vf->compHeight;
			} else {
				pic_w = dec_vf->width;
				pic_h = dec_vf->height;
			}
		}

		if ((pic_w == 0 || pic_h == 0) && dec_vf->vf_ext) {
			vfp = dec_vf->vf_ext;
			pic_w = vfp->width;
			pic_h = vfp->height;
		}

		if (video->vfm_mode) {
			struct dma_fence *old_fence = NULL;

			vf_info.release_fence = video->fence;
			vf_info.dmabuf = mvvs->dmabuf;
			vf_info.dst_x = mvvs->dst_x;
			vf_info.dst_y = mvvs->dst_y;
			vf_info.dst_w = mvvs->dst_w;
			vf_info.dst_h = mvvs->dst_h;
			vf_info.crop_x = vf->crop[1];
			vf_info.crop_y = vf->crop[0];
			vf_info.crop_w = recal_src_w;
			vf_info.crop_h = recal_src_h;
			vf_info.buffer_w = pic_w;
			vf_info.buffer_h = pic_h;
			vf_info.zorder = mvvs->zorder;
			vf_info.reserved[0] = 0;
			vf_info.phy_addr[0] = mvvs->phy_addr[0];
			vf_info.phy_addr[1] = mvvs->phy_addr[1];

			if (vf_info.dmabuf && vf_info.dmabuf->resv)
				old_fence = dma_resv_get_excl(vf_info.dmabuf->resv);

			if (repeat_frame) {
				MESON_DRM_FENCE("dmabuf(%px), fence%px,old_fence=%px-%d re-frame\n",
					vf_info.dmabuf, vf_info.release_fence, old_fence,
					old_fence ? kref_read(&old_fence->refcount) : -1);
				dma_fence_get(vf_info.release_fence);
			} else {
				MESON_DRM_FENCE("dmabuf(%px), fence%px, old_fence=%px-%d\n",
					vf_info.dmabuf, vf_info.release_fence, old_fence,
					old_fence ? kref_read(&old_fence->refcount) : -1);
				if (vf_info.dmabuf && vf_info.dmabuf->resv)
					dma_resv_add_excl_fence(vf_info.dmabuf->resv,
						vf_info.release_fence);
			}

			MESON_DRM_BLOCK("vf-info crop:%u, %u, %u, %u, pic:%u, %u\n",
				vf_info.crop_x, vf_info.crop_y, vf_info.crop_w, vf_info.crop_h,
				vf_info.buffer_w, vf_info.buffer_h);
			video_display_setframe(vblk->index, &vf_info, 0);
		} else {
			/*crop bottow*/
			if (pic_h > recal_src_h + vf->crop[0])
				vf->crop[2] = pic_h - recal_src_h - vf->crop[0];
			else
				vf->crop[2] = 0;

			/*crop right*/
			if (pic_w > recal_src_w + vf->crop[1])
				vf->crop[3] = pic_w - recal_src_w - vf->crop[1];
			else
				vf->crop[3] = 0;
			MESON_DRM_BLOCK("vf-crop:%u, %u, %u, %u\n",
					pic_w, pic_h, vf->crop[2], vf->crop[3]);
			vf->flag |= VFRAME_FLAG_VIDEO_DRM;
			if (!kfifo_put(&video->ready_q, vf))
				DRM_INFO("ready_q is full!\n");
		}
	} else {
		if (video->vfm_mode) {
			struct dma_fence *old_fence = NULL;

			vf_info.release_fence = video->fence;
			video_vfm_convert_to_vfminfo(mvvs, &vf_info);
			vf_info.phy_addr[0] = mvvs->phy_addr[0];
			vf_info.phy_addr[1] = mvvs->phy_addr[1];
			vf_info.reserved[0] = video_type_get(pixel_format);

			if (vf_info.dmabuf && vf_info.dmabuf->resv)
				old_fence = dma_resv_get_excl(vf_info.dmabuf->resv);

			if (repeat_frame) {
				MESON_DRM_FENCE("dmabuf(%px),fence=%px,old_fence=%px-%d re-frame\n",
					vf_info.dmabuf, vf_info.release_fence, old_fence,
					old_fence ? kref_read(&old_fence->refcount) : -1);
				dma_fence_get(vf_info.release_fence);
			} else {
				MESON_DRM_FENCE("dmabuf(%px), fence%px, old_fence=%px-%d\n",
					vf_info.dmabuf, vf_info.release_fence, old_fence,
					old_fence ? kref_read(&old_fence->refcount) : -1);
				if (vf_info.dmabuf && vf_info.dmabuf->resv)
					dma_resv_add_excl_fence(vf_info.dmabuf->resv,
						vf_info.release_fence);
			}
			video_display_setframe(vblk->index, &vf_info, 0);
		} else {
			if (pixel_format == DRM_FORMAT_NV12 ||
			    pixel_format == DRM_FORMAT_NV21) {
				if (!mvvs->phy_addr[1])
					phy_addr2 = phy_addr + byte_stride * src_h;
				else
					phy_addr2 = mvvs->phy_addr[1];
			}

			if (kfifo_get(&video->free_q, &vf) && vf) {
				memset(vf, 0, sizeof(struct vframe_s));
				vf->width = mvvs->src_w;
				vf->height = mvvs->src_h;
				vf->source_type = VFRAME_SOURCE_TYPE_OTHERS;
				vf->source_mode = VFRAME_SOURCE_MODE_OTHERS;
				vf->bitdepth = BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
				vf->type = video_type_get(pixel_format);
				vf->axis[0] = mvvs->dst_x;
				vf->axis[1] = mvvs->dst_y;
				vf->axis[2] = mvvs->dst_x + mvvs->dst_w - 1;
				vf->axis[3] = mvvs->dst_y + mvvs->dst_h - 1;
				vf->crop[0] = mvvs->src_y;/*crop top*/
				vf->crop[1] = mvvs->src_x;/*crop left*/
				/*vf->width is from mvvs->src_w which is the
				 *valid content so the crop of bottom and right
				 *could be 0
				 */
				vf->crop[2] = 0;/*crop bottow*/
				vf->crop[3] = 0;/*crop right*/
				vf->flag |= VFRAME_FLAG_VIDEO_DRM;
				/*need sync with vpp*/
				vf->canvas0Addr = (u32)-1;
				/*Todo: if canvas0_config.endian = 1
				 *supprot little endian is okay,could be removed.
				 */
				vf->flag |= VFRAME_FLAG_VIDEO_LINEAR;
				vf->plane_num = 1;
				vf->canvas0_config[0].phy_addr = phy_addr;
				vf->canvas0_config[0].width = byte_stride;
				vf->canvas0_config[0].height = src_h;
				vf->canvas0_config[0].block_mode =
					CANVAS_BLKMODE_LINEAR;
				/*big endian default support*/
				vf->canvas0_config[0].endian = 0;
				if (pixel_format == DRM_FORMAT_NV12 ||
				    pixel_format == DRM_FORMAT_NV21) {
					vf->plane_num = 2;
					vf->canvas0_config[1].phy_addr = phy_addr2;
					vf->canvas0_config[1].width = byte_stride;
					vf->canvas0_config[1].height = src_h / 2;
					vf->canvas0_config[1].block_mode =
						CANVAS_BLKMODE_LINEAR;
					/*big endian default support*/
					vf->canvas0_config[1].endian = 0;
				}
				MESON_DRM_BLOCK("vframe info:type(0x%x),plane_num=%d\n",
					  vf->type, vf->plane_num);
				if (!kfifo_put(&video->ready_q, vf))
					DRM_INFO("ready_q is full!\n");
			} else {
				DRM_INFO("free_q get fail!");
			}
		}
	}
	if (!video->vfm_mode && video->fence && vf)
		bind_video_fence_vframe(video, video->fence,
				mvvs->plane_index, vf);
	MESON_DRM_BLOCK("plane_index=%d,HW-video=%d, repeat_frame=%d byte_stride=%d\n",
		  mvvs->plane_index, vblk->index, repeat_frame, byte_stride);
	MESON_DRM_BLOCK("phy_addr=0x%pa,phy_addr2=0x%pa\n",
		  &phy_addr, &phy_addr2);
	MESON_DRM_BLOCK("%s set_state done.\n", video->base.name);
}

static void video_hw_enable(struct meson_vpu_block *vblk,
			    struct meson_vpu_block_state *state)
{
	struct meson_vpu_video *video = to_video_block(vblk);

	if (!video) {
		MESON_DRM_BLOCK("enable break for NULL.\n");
		return;
	}

	if (video->vfm_mode) {
		MESON_DRM_BLOCK("skip, %s enable by video_composer.\n", video->base.name);
		return;
	}

	if (!video->video_enabled) {
		set_video_enabled(1, vblk->index);
		video->video_enabled = 1;
	}
	MESON_DRM_BLOCK("%s enable done.\n", video->base.name);
}

static void video_hw_disable(struct meson_vpu_block *vblk,
			     struct meson_vpu_block_state *state)
{
	struct meson_vpu_video *video = to_video_block(vblk);

	if (!video) {
		MESON_DRM_BLOCK("disable break for NULL.\n");
		return;
	}

	if (video->vfm_mode) {
		video_display_setenable(vblk->index, 0);
		video->video_enabled = 0;
		video->fence = NULL;
	} else {
		video_disable_fence(video);
		video->fence = NULL;
		if (video->video_enabled) {
			set_video_enabled(0, vblk->index);
			video->video_enabled = 0;
		}

		if (video->video_path_reg) {
			vf_unreg_provider(&video->vprov);
			video->video_path_reg = 0;
		}
	}
	MESON_DRM_BLOCK("%s disable done.\n", video->base.name);
}

static void video_dump_register(struct meson_vpu_block *vblk,
				struct seq_file *seq)
{
}

static void video_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_video *video = to_video_block(vblk);
	int i;

	if (!vblk || !video) {
		MESON_DRM_BLOCK("%s break for NULL.\n", __func__);
		return;
	}

	if (!video->vfm_mode) {
		for (i = 0; i < MESON_MAX_VIDEO; i++)
			INIT_LIST_HEAD(&video->vfm_node[i]);
		INIT_KFIFO(video->ready_q);
		INIT_KFIFO(video->free_q);
		INIT_KFIFO(video->display_q);
		kfifo_reset(&video->ready_q);
		kfifo_reset(&video->free_q);
		kfifo_reset(&video->display_q);
		for (i = 0; i < BUFFER_NUM; i++)
			kfifo_put(&video->free_q, &video->vframe[i]);
		if (vblk->id == VIDEO1_BLOCK)
			snprintf(video->vfm_map_chain, VP_MAP_STRUCT_SIZE,
				 "%s %s", video->base.name,
				 "amvideo");
		else if (vblk->id == VIDEO2_BLOCK)
			snprintf(video->vfm_map_chain, VP_MAP_STRUCT_SIZE,
				 "%s %s", video->base.name,
				 "videopip");
		else if (vblk->id == VIDEO3_BLOCK)
			snprintf(video->vfm_map_chain, VP_MAP_STRUCT_SIZE,
				"%s %s", video->base.name,
				"videopip2");
		else
			MESON_DRM_BLOCK("unsupported block id %d\n", vblk->id);

		snprintf(video->vfm_map_id, VP_MAP_STRUCT_SIZE,
			 "video-map-%d", vblk->index);
		vfm_map_add(video->vfm_map_id, video->vfm_map_chain);
		vf_provider_init(&video->vprov, video->base.name,
					 &vp_vf_ops, video);
	}
	MESON_DRM_BLOCK("%s:%s done.\n", __func__, video->base.name);
}

struct meson_vpu_block_ops video_ops = {
	.check_state = video_check_state,
	.update_state = video_set_state,
	.enable = video_hw_enable,
	.disable = video_hw_disable,
	.dump_register = video_dump_register,
	.init = video_hw_init,
};
