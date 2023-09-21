// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/amlogic/meson_uvm_core.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_sink/v4lvideo_ext.h>

#include "meson_uvm_buffer_info.h"

static int mubi_debug_level;
module_param(mubi_debug_level, int, 0644);
#define MUBI_PRINTK(level, fmt, arg...) \
	do { \
		if (mubi_debug_level >= (level)) \
			pr_info("MUBI: " fmt, ## arg); \
	} while (0)

static bool is_dv_video(const struct vframe_s *vfp)
{
	/* dolby vision: bit 30 */
	/*
	u32 dv_flag = (1 << 30);

	if (vfp->signal_type & dv_flag)
		return true;
	*/

	if (!vfp->discard_dv_data)
		return true;

	return false;
}

static bool is_hdr_video(const struct vframe_s *vfp)
{
	if (vfp->src_fmt.sei_magic_code == SEI_MAGIC_CODE) {
		if (vfp->src_fmt.fmt == VFRAME_SIGNAL_FMT_HDR10)
			return true;
	}

	return false;
}

static bool is_hdr10_plus_video(const struct vframe_s *vfp)
{
	if (vfp->src_fmt.sei_magic_code == SEI_MAGIC_CODE) {
		if (vfp->src_fmt.fmt == VFRAME_SIGNAL_FMT_HDR10PLUS ||
			vfp->src_fmt.fmt == VFRAME_SIGNAL_FMT_HDR10PRIME)
			return true;
	}

	return false;
}

static bool is_hlg_video(const struct vframe_s *vfp)
{
	if (vfp->src_fmt.sei_magic_code == SEI_MAGIC_CODE) {
		if (vfp->src_fmt.fmt == VFRAME_SIGNAL_FMT_HLG)
			return true;
	}

	return false;
}

static bool is_secure_video(const struct vframe_s *vfp)
{
	if (vfp->flag & VFRAME_FLAG_VIDEO_SECURE)
		return true;

	return false;
}

static bool is_afbc_video(const struct vframe_s *vfp)
{
	if (vfp->compWidth > 0 && vfp->compHeight > 0)
		return true;

	return false;
}

static enum uvm_video_type get_resolution_vframe(const struct vframe_s *vfp)
{
	u32 h, w;

	h = max(vfp->height, vfp->compHeight);
	w = max(vfp->width, vfp->compWidth);

	if (h > 2160 || w > 3840)
		return AM_VIDEO_8K;
	else if (h > 1080 || w > 1920)
		return AM_VIDEO_4K;

	return AM_VIDEO_NORMAL;
}

static struct vframe_s *get_vf_from_dma(struct dma_buf *dma_buf)
{
	bool is_dec_vf = false;
	bool is_v4l_vf = false;
	struct vframe_s *p_vf = NULL;
	struct uvm_hook_mod *uhmod = NULL;
	struct file_private_data *file_private_data = NULL;

	is_dec_vf = is_valid_mod_type(dma_buf, VF_SRC_DECODER);
	is_v4l_vf = is_valid_mod_type(dma_buf, VF_PROCESS_V4LVIDEO);

	if (is_dec_vf) {
		p_vf = dmabuf_get_vframe(dma_buf);
		if (!p_vf)
			MUBI_PRINTK(1, "%s, get vframe for decoder fail\n", __func__);

		dmabuf_put_vframe(dma_buf);
	} else if (is_v4l_vf) {
		uhmod = uvm_get_hook_mod(dma_buf, VF_PROCESS_V4LVIDEO);
		if (IS_ERR_OR_NULL(uhmod) || !uhmod->arg) {
			MUBI_PRINTK(1, "%s, dma file file_private_data is NULL\n", __func__);
			goto exit_ret;
		}

		file_private_data = uhmod->arg;
		uvm_put_hook_mod(dma_buf, VF_PROCESS_V4LVIDEO);

		if (!file_private_data)
			MUBI_PRINTK(1, "%s, invalid buffer fd, cannot found dma file", __func__);
		else
			p_vf = &file_private_data->vf;
	} else {
		MUBI_PRINTK(1, "%s, buffer is not UVM, not V4lvideo", __func__);
	}

exit_ret:
	return p_vf;
}

int get_uvm_video_info(const int fd, int *videotype, u64 *timestamp)
{
	struct dma_buf *dmabuf;
	struct vframe_s *vfp;
	enum uvm_video_type type = AM_VIDEO_NORMAL;
	int ret = 0;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		MUBI_PRINTK(1, "%s, invalid dmabuf fd\n", __func__);
		return -EINVAL;
	}

	if (!dmabuf_is_uvm(dmabuf)) {
		MUBI_PRINTK(1, "%s, dmabuf is not uvm\n", __func__);
		dma_buf_put(dmabuf);
		return -EINVAL;
	}

	vfp = get_vf_from_dma(dmabuf);
	if (IS_ERR_OR_NULL(vfp)) {
		MUBI_PRINTK(1, "%s, vframe is null.\n", __func__);
		ret = -EINVAL;
		goto exit_ret;
	}

	if (is_dv_video(vfp))
		type |= AM_VIDEO_DV;

	if (is_hdr_video(vfp))
		type |= AM_VIDEO_HDR;

	if (is_hdr10_plus_video(vfp))
		type |= AM_VIDEO_HDR10_PLUS;

	if (is_hlg_video(vfp))
		type |= AM_VIDEO_HLG;

	if (is_secure_video(vfp))
		type |= AM_VIDEO_SECURE;

	if (is_afbc_video(vfp))
		type |= AM_VIDEO_AFBC;

	type |= get_resolution_vframe(vfp);

	*videotype = (int)type;
	*timestamp = vfp->timestamp;

	MUBI_PRINTK(1, "%s, videotype:0x%x timestamp:%lld\n",
			__func__, *videotype, *timestamp);
exit_ret:
	dma_buf_put(dmabuf);

	return ret;
}
