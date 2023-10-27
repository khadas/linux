// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/meson_uvm_core.h>
#include <linux/amlogic/media/video_sink/v4lvideo_ext.h>
#include <linux/amlogic/media/vpq/vpq.h>
#include "vpq_processor.h"
#include "vpq_printk.h"
#include "vpq_vfm.h"

int vpq_process_set_frame(struct vpq_dev_s *dev, struct vpq_frame_info_s *frame_info)
{
	struct uvm_hook_mod *uhmod = NULL;
	struct dma_buf *dmabuf = NULL;
	bool is_dec_vf = false, is_v4l_vf = false;
	struct vframe_s *vf = NULL;
	struct vframe_s *di_vf = NULL;
	struct file_private_data *file_private_data = NULL;
	int shared_fd = frame_info->shared_fd;
	int interlace_mode = 0;

	dmabuf = dma_buf_get(shared_fd);

	if (IS_ERR_OR_NULL(dmabuf)) {
		VPQ_PR_ERR("%s: Invalid dmabuf\n", __func__);
		return -EINVAL;
	}

	if (!dmabuf_is_uvm(dmabuf)) {
		VPQ_PR_ERR("%s: dmabuf is not uvm.dmabuf=%px, shared_fd=%d\n",
			__func__, dmabuf, shared_fd);
		dma_buf_put(dmabuf);
		return -EINVAL;
	}

	is_dec_vf = is_valid_mod_type(dmabuf, VF_SRC_DECODER);
	is_v4l_vf = is_valid_mod_type(dmabuf, VF_PROCESS_V4LVIDEO);

	if (is_dec_vf) {
		vf = dmabuf_get_vframe(dmabuf);
		if (IS_ERR_OR_NULL(vf)) {
			VPQ_PR_ERR("%s: vf is NULL\n", __func__);
			return -EINVAL;
		}

		VPQ_PR_INFO(PR_PROC, "%s vf:%d %d,flag:%x,type:%x\n",
			__func__, vf->width, vf->height, vf->flag, vf->type);

		di_vf = vf->vf_ext;
		interlace_mode = vf->type & VIDTYPE_TYPEMASK;
		if (di_vf && (vf->flag & VFRAME_FLAG_CONTAIN_POST_FRAME)) {
			if (interlace_mode != VIDTYPE_PROGRESSIVE) {
				/*for interlace*/
				VPQ_PR_INFO(PR_PROC, "%s vf:%d %d,flag:%x,type:%x\n",
					__func__, vf->width, vf->height, vf->flag, vf->type);
				vf = di_vf;
			}
		}
		dmabuf_put_vframe(dmabuf);
	} else {
		uhmod = uvm_get_hook_mod(dmabuf, VF_PROCESS_V4LVIDEO);
		if (IS_ERR_OR_NULL(uhmod) || !uhmod->arg) {
			VPQ_PR_ERR("%s: get dw vf err: no v4lvideo\n", __func__);
			dma_buf_put(dmabuf);
			return -EINVAL;
		}
		file_private_data = uhmod->arg;
		uvm_put_hook_mod(dmabuf, VF_PROCESS_V4LVIDEO);
		if (!file_private_data) {
			VPQ_PR_ERR("%s: invalid fd no uvm/v4lvideo\n", __func__);
		} else {
			vf = &file_private_data->vf;
			if (vf->vf_ext)
				vf = vf->vf_ext;
		}
	}
	if (!vf) {
		VPQ_PR_ERR("%s: not find vf\n", __func__);
		dma_buf_put(dmabuf);
		return -EINVAL;
	}
	dma_buf_put(dmabuf);

	vpq_vfm_process(vf);
	VPQ_PR_INFO(PR_PROC, "%s %d 0x%x 0x%x 0x%x %d %d %px\n",
		__func__,
		vf->source_type, vf->sig_fmt, vf->port,
		vf->signal_type, vf->type, vf->type_original, dev);

	return 1;
}

int vpq_processor_set_frame_status(enum vpq_frame_status_e status)
{
	if (status == VPQ_VFRAME_STOP)
		vpq_vfm_video_enable(0);

	return 1;
}
