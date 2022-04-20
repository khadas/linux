// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * drivers/amlogic/media/video_sink/video_receiver.c
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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/ctype.h>
#include <linux/sched.h>
#include <linux/slab.h>

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#include "../common/rdma/rdma.h"
#endif
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/video_sink/vpp.h>
#include <linux/amlogic/media/video_sink/video.h>
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#endif
#include <linux/amlogic/media/di/di.h>

#include "video_receiver.h"

/* #define ENABLE_DV */

/*********************************************************
 * vframe APIs
 *********************************************************/
static inline struct vframe_s *common_vf_peek(struct video_recv_s *ins)
{
	struct vframe_s *vf = NULL;

	if (!ins || !ins->recv_name || !ins->active)
		return NULL;

	vf = vf_peek(ins->recv_name);
	if (vf && vf->disp_pts && vf->disp_pts_us64) {
		vf->pts = vf->disp_pts;
		vf->pts_us64 = vf->disp_pts_us64;
		vf->disp_pts = 0;
		vf->disp_pts_us64 = 0;
	}
	return vf;
}

static inline struct vframe_s *common_vf_get(struct video_recv_s *ins)
{
	struct vframe_s *vf = NULL;

	if (!ins || !ins->recv_name || !ins->active)
		return NULL;

	vf = vf_get(ins->recv_name);

	if (vf && debug_flag & DEBUG_FLAG_VPP_GET_BUFFER_TIME)
		pr_info("vpp_get buf_idx:%d vpp_cur_time:%lld\n",
		vf->index_disp, ktime_to_us(ktime_get()));

	if (vf) {
		if (!tvin_vf_disp_mode_check(vf)) {
			vf_put(vf, ins->recv_name);
			return NULL;
		}
		if (vf->type & VIDTYPE_V4L_EOS) {
			vf_put(vf, ins->recv_name);
			return NULL;
		}
		if (vf->disp_pts && vf->disp_pts_us64) {
			vf->pts = vf->disp_pts;
			vf->pts_us64 = vf->disp_pts_us64;
			vf->disp_pts = 0;
			vf->disp_pts_us64 = 0;
		}
		ins->notify_flag |= VIDEO_NOTIFY_PROVIDER_GET;
		pre_process_for_3d(vf);
	}
	return vf;
}

#ifdef MORE_FUNCTION
static int common_vf_get_states(struct video_recv_s *ins,
				struct vframe_states *states)
{
	int ret = -1;
	unsigned long flags;

	if (!ins || !ins->recv_name || !ins->active || !states)
		return ret;

	spin_lock_irqsave(&ins->lock, flags);
	ret = vf_get_states_by_name(ins->recv_name, states);
	spin_unlock_irqrestore(&ins->lock, flags);
	return ret;
}
#endif

static inline void common_vf_put(struct video_recv_s *ins,
				 struct vframe_s *vf)
{
	struct vframe_provider_s *vfp = NULL;

	if (!ins || !ins->recv_name)
		return;

	vfp = vf_get_provider(ins->recv_name);
	if (vfp && vf) {
		vf_put(vf, ins->recv_name);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		if (glayer_info[0].display_path_id == ins->path_id &&
		    is_dolby_vision_enable())
			dolby_vision_vf_put(vf);
#endif
		ins->notify_flag |= VIDEO_NOTIFY_PROVIDER_PUT;
	}
}

/* TODO: need add keep frame function */
static void common_vf_unreg_provider(struct video_recv_s *ins)
{
	ulong flags;
	bool layer1_used = false;
	bool layer2_used = false;
	bool layer3_used = false;
	int i;
	bool vpp1_used = false, vpp2_used = false;
	bool wait = false;

	if (!ins)
		return;
	if (!strcmp(ins->recv_name, "video_render.5"))
		vpp1_used = true;
	if (!strcmp(ins->recv_name, "video_render.6"))
		vpp2_used = true;
	/* FIXME: remove the global variable */
	atomic_inc(&video_unreg_flag);
	if (vpp1_used)
		atomic_inc(&video_unreg_flag_vpp[0]);
	if (vpp2_used)
		atomic_inc(&video_unreg_flag_vpp[1]);
	while (atomic_read(&video_inirq_flag) > 0)
		schedule();
	if (vpp1_used)
		while (atomic_read(&video_inirq_flag_vpp[0]) > 0)
			schedule();
	if (vpp2_used)
		while (atomic_read(&video_inirq_flag_vpp[1]) > 0)
			schedule();

	spin_lock_irqsave(&ins->lock, flags);
	ins->buf_to_put_num = 0;
	for (i = 0; i < DISPBUF_TO_PUT_MAX; i++)
		ins->buf_to_put[i] = NULL;
	ins->rdma_buf = NULL;
	ins->original_vf = NULL;
	ins->switch_vf = false;
	ins->last_switch_state = false;
	ins->frame_count = 0;

	if (ins->cur_buf) {
		if (ins->cur_buf->vf_ext &&
		    IS_DI_POSTWRTIE(ins->cur_buf->type)) {
			struct vframe_s *tmp =
				(struct vframe_s *)ins->cur_buf->vf_ext;

			memcpy(&tmp->pic_mode, &ins->cur_buf->pic_mode,
				sizeof(struct vframe_pic_mode_s));
			ins->local_buf_ext = *tmp;
			ins->local_buf = *ins->cur_buf;
			ins->local_buf.vf_ext = (void *)&ins->local_buf_ext;
		} else {
			ins->local_buf = *ins->cur_buf;
		}
		ins->cur_buf = &ins->local_buf;
	}
	spin_unlock_irqrestore(&ins->lock, flags);

	if (vd_layer[0].dispbuf_mapping
		== &ins->cur_buf) {
		layer1_used = true;
	}
	if (vd_layer_vpp[0].vpp_index != VPP0 && vd_layer_vpp[0].layer_id == 1) {
		if (vd_layer_vpp[0].dispbuf_mapping
			== &ins->cur_buf) {
			layer2_used = true;
		}
	} else {
		if (vd_layer[1].dispbuf_mapping
			== &ins->cur_buf) {
			layer2_used = true;
		}
	}

	if (vd_layer_vpp[1].vpp_index != VPP0 && vd_layer_vpp[1].layer_id == 2) {
		if (vd_layer_vpp[0].dispbuf_mapping
			== &ins->cur_buf) {
			layer3_used = true;
		}
	} else {
		if (vd_layer[2].dispbuf_mapping
			== &ins->cur_buf) {
			layer3_used = true;
		}
	}

	if (layer1_used || !vd_layer[0].dispbuf_mapping)
		atomic_set(&primary_src_fmt, VFRAME_SIGNAL_FMT_INVALID);

	if (!layer1_used && !layer2_used) {
		ins->cur_buf = NULL;
	} else {
		ins->request_exit = true;
		ins->do_exit = false;
		ins->exited = false;
		wait = true;
	}

	if (layer1_used)
		safe_switch_videolayer(0, false, true);
	if (layer2_used)
		safe_switch_videolayer(1, false, true);
	if (layer3_used)
		safe_switch_videolayer(2, false, true);

	pr_info("%s %s: vd1 used: %s, vd2 used: %s, vd3 used: %s, black_out:%d, cur_buf:%p\n",
		__func__,
		ins->recv_name,
		layer1_used ? "true" : "false",
		layer2_used ? "true" : "false",
		layer3_used ? "true" : "false",
		ins->blackout | force_blackout,
		ins->cur_buf);

	ins->active = false;
	atomic_dec(&video_unreg_flag);
	if (vpp1_used)
		atomic_dec(&video_unreg_flag_vpp[0]);
	if (vpp2_used)
		atomic_dec(&video_unreg_flag_vpp[1]);

	while (wait && (!ins->exited || ins->request_exit) && !video_suspend)
		schedule();
}

static void common_vf_light_unreg_provider(struct video_recv_s *ins)
{
	ulong flags;
	int i;
	bool vpp1_used = false, vpp2_used = false;

	if (!ins)
		return;

	if (!strcmp(ins->recv_name, "video_render.5"))
		vpp1_used = true;
	if (!strcmp(ins->recv_name, "video_render.6"))
		vpp2_used = true;
	/* FIXME: remove the global variable */
	atomic_inc(&video_unreg_flag);
	if (vpp1_used)
		atomic_inc(&video_unreg_flag_vpp[0]);
	if (vpp2_used)
		atomic_inc(&video_unreg_flag_vpp[1]);
	while (atomic_read(&video_inirq_flag) > 0)
		schedule();
	if (vpp1_used)
		while (atomic_read(&video_inirq_flag_vpp[0]) > 0)
			schedule();
	if (vpp2_used)
		while (atomic_read(&video_inirq_flag_vpp[1]) > 0)
			schedule();
	spin_lock_irqsave(&ins->lock, flags);
	ins->buf_to_put_num = 0;
	for (i = 0; i < DISPBUF_TO_PUT_MAX; i++)
		ins->buf_to_put[i] = NULL;
	ins->rdma_buf = NULL;

	if (ins->cur_buf) {
		ins->local_buf = *ins->cur_buf;
		ins->cur_buf = &ins->local_buf;
	}
	spin_unlock_irqrestore(&ins->lock, flags);

	atomic_dec(&video_unreg_flag);
	if (vpp1_used)
		atomic_dec(&video_unreg_flag_vpp[0]);
	if (vpp2_used)
		atomic_dec(&video_unreg_flag_vpp[1]);
}

static int common_receiver_event_fun(int type,
				     void *data,
				     void *private_data)
{
	struct video_recv_s *ins = (struct video_recv_s *)private_data;

	if (!ins) {
		pr_err("common_receiver_event: ins invalid, type:%d\n", type);
		return -1;
	}
	if (type == VFRAME_EVENT_PROVIDER_UNREG) {
		common_vf_unreg_provider(ins);
		atomic_dec(&video_recv_cnt);
	} else if (type == VFRAME_EVENT_PROVIDER_RESET) {
		common_vf_light_unreg_provider(ins);
	} else if (type == VFRAME_EVENT_PROVIDER_LIGHT_UNREG) {
		common_vf_light_unreg_provider(ins);
	} else if (type == VFRAME_EVENT_PROVIDER_REG) {
		atomic_inc(&video_recv_cnt);
		common_vf_light_unreg_provider(ins);
		ins->drop_vf_cnt = 0;
		ins->active = true;
		ins->request_exit = false;
		ins->do_exit = false;
		ins->exited = false;
	}
	return 0;
}

static inline int recycle_dvel_vf_put(void)
{
#define DVEL_RECV_NAME "dvel"

	int event = 0, ret = 0;
	struct vframe_s *vf = NULL;
	struct vframe_provider_s *vfp = vf_get_provider(DVEL_RECV_NAME);

	if (vfp) {
		vf = vf_get(DVEL_RECV_NAME);
		if (vf) {
			vf_put(vf, DVEL_RECV_NAME);
			event |= VFRAME_EVENT_RECEIVER_PUT;
			vf_notify_provider(DVEL_RECV_NAME, event, NULL);
			ret = 1;
		}
	}
	return ret;
}

static const struct vframe_receiver_op_s common_recv_func = {
	.event_cb = common_receiver_event_fun
};

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
static int dolby_vision_need_wait_common(struct video_recv_s *ins)
{
	struct vframe_s *vf;

	if (!is_dolby_vision_enable() || !ins)
		return 0;

	vf = common_vf_peek(ins);
	if (!vf || (dolby_vision_wait_metadata(vf) == 1))
		return 1;
	return 0;
}
#endif

static void common_toggle_frame(struct video_recv_s *ins,
				struct vframe_s *vf)
{
	if (!ins || !vf)
		return;

	if (vf->width == 0 || vf->height == 0) {
		pr_err("%s %s: invalid frame dimension\n",
		       __func__,
		       ins->recv_name);
		return;
	}
	if (ins->cur_buf &&
	    ins->cur_buf != &ins->local_buf &&
	    (ins->original_vf != vf)) {
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
		int i = 0;

		if (is_vsync_rdma_enable()) {
			if (ins->rdma_buf == ins->cur_buf) {
				if (ins->buf_to_put_num < DISPBUF_TO_PUT_MAX) {
					ins->buf_to_put[ins->buf_to_put_num] =
					    ins->original_vf;
					ins->buf_to_put_num++;
				} else {
					common_vf_put(ins, ins->original_vf);
				}
			} else {
				common_vf_put(ins, ins->original_vf);
			}
		} else {
			for (i = 0; i < ins->buf_to_put_num; i++) {
				if (ins->buf_to_put[i]) {
					common_vf_put(ins, ins->buf_to_put[i]);
					ins->buf_to_put[i] = NULL;
				}
			}
			ins->buf_to_put_num = 0;
			common_vf_put(ins, ins->original_vf);
		}
#else
		common_vf_put(ins, ins->original_vf);
#endif
		ins->frame_count++;
	}
	if (ins->original_vf != vf)
		vf->type_backup = vf->type;
	ins->cur_buf = vf;
	ins->original_vf = vf;
}

/*********************************************************
 * recv func APIs
 *********************************************************/
static s32 recv_common_early_process(struct video_recv_s *ins, u32 op)
{
	int i;

	if (!ins) {
		pr_err("%s error, empty ins\n", __func__);
		return -1;
	}

	/* not over vsync */
	if (!op) {
		for (i = 0; i < ins->buf_to_put_num; i++) {
			if (ins->buf_to_put[i]) {
				ins->buf_to_put[i]->rendered = true;
				common_vf_put(ins, ins->buf_to_put[i]);
				ins->buf_to_put[i] = NULL;
			}
		}
		ins->buf_to_put_num = 0;
	}
	if (ins->do_exit) {
		ins->exited = true;
		ins->do_exit = false;
	}
	return 0;
}

static struct vframe_s *recv_common_dequeue_frame(struct video_recv_s *ins,
								struct path_id_s *path_id)
{
	struct vframe_s *vf = NULL;
	struct vframe_s *toggle_vf = NULL;
	s32 drop_count = -1;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	enum vframe_signal_fmt_e fmt;
#endif

	if (!ins) {
		pr_err("%s error, empty ins\n", __func__);
		return NULL;
	}

	if (ins->request_exit) {
		ins->do_exit = true;
		ins->exited = false;
		ins->request_exit = false;
	}
	vf = common_vf_peek(ins);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (glayer_info[0].display_path_id == ins->path_id &&
	    is_dolby_vision_enable()) {
		struct provider_aux_req_s req;
		u32 i, bl_cnt = 0xffffffff;

		if (vf) {
			dolby_vision_check_mvc(vf);
			dolby_vision_check_hdr10(vf);
			dolby_vision_check_hdr10plus(vf);
			dolby_vision_check_hlg(vf);
			dolby_vision_check_primesl(vf);
		}

		fmt = get_vframe_src_fmt(vf);
		if ((fmt == VFRAME_SIGNAL_FMT_DOVI ||
		     fmt == VFRAME_SIGNAL_FMT_INVALID) &&
		    vf_peek("dvel")) {
			req.vf = NULL;
			req.bot_flag = 0;
			req.aux_buf = NULL;
			req.aux_size = 0xffffffff;
			req.dv_enhance_exist = 0;
			vf_notify_provider_by_name
				("dvbldec",
				VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
				(void *)&req);
			bl_cnt = req.aux_size;

			req.vf = NULL;
			req.bot_flag = 0;
			req.aux_buf = NULL;
			req.aux_size = 0xffffffff;
			req.dv_enhance_exist = 0;
			vf_notify_provider_by_name
				("dveldec",
				VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
				(void *)&req);
			if (req.aux_size != 0xffffffff &&
			    bl_cnt != 0xffffffff &&
			    bl_cnt > req.aux_size) {
				i = bl_cnt - req.aux_size;
				while (i > 0) {
					if (!recycle_dvel_vf_put())
						break;
					i--;
				}
			}
		}
	}
#endif
	while (vf) {
		if (!vf->frame_dirty) {
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			if (glayer_info[0].display_path_id == ins->path_id &&
			    dolby_vision_need_wait_common(ins))
				break;
#endif
			vf = common_vf_get(ins);
			if (vf) {
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
				amvecm_process(path_id, ins, vf);
#endif
				common_toggle_frame(ins, vf);
				toggle_vf = vf;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
				if (glayer_info[0].display_path_id ==
				    ins->path_id)
					dvel_toggle_frame(vf, true);
#endif
			}
		} else {
			vf = common_vf_get(ins);
			if (vf)
				common_vf_put(ins, vf);
		}
		drop_count++;
		vf = common_vf_peek(ins);
	}
#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
	if (toggle_vf && IS_DI_POST(toggle_vf->type) &&
	    (toggle_vf->flag & VFRAME_FLAG_DOUBLE_FRAM) &&
	    glayer_info[0].display_path_id == ins->path_id) {
		if (toggle_vf->di_instance_id == di_api_get_instance_id()) {
			if (ins->switch_vf) {
				ins->switch_vf = false;
				pr_info("set switch_vf false\n");
			}
		} else {
			if (!ins->switch_vf) {
				ins->switch_vf = true;
				pr_info("set switch_vf true\n");
			}
		}
	}
#endif

	if (toggle_vf && (toggle_vf->flag & VFRAME_FLAG_DOUBLE_FRAM)) {
		/* new frame */
		if (ins->switch_vf && toggle_vf->vf_ext) {
			ins->cur_buf = toggle_vf->vf_ext;
			toggle_vf = ins->cur_buf;
		}
	} else if (ins->cur_buf) {
		/* repeat frame */
		if (ins->switch_vf && ins->cur_buf->vf_ext &&
		    (ins->cur_buf->flag & VFRAME_FLAG_DOUBLE_FRAM)) {
			ins->cur_buf = ins->cur_buf->vf_ext;
			toggle_vf = ins->cur_buf;
		} else if (!ins->switch_vf &&
			(ins->cur_buf != ins->original_vf)) {
			ins->cur_buf = ins->original_vf;
			toggle_vf = ins->cur_buf;
		}
	}
#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
	if (ins->switch_vf &&
	    ins->switch_vf != ins->last_switch_state) {
		di_api_post_disable();
	}
#endif

	ins->last_switch_state = ins->switch_vf;
	return toggle_vf;
}

static s32 recv_common_return_frame(struct video_recv_s *ins,
				    struct vframe_s *vf)
{
	if (!ins || !vf) {
		pr_err("%s error, empty ins\n", __func__);
		return -1;
	}

	if (vf == ins->cur_buf &&
	    ins->cur_buf == &ins->local_buf) {
		pr_info("recv_common_return_frame skip the local vf =%p\n",
			ins->original_vf);
		return 0;
	}

	if (vf == ins->cur_buf) {
		common_vf_put(ins, ins->original_vf);
		ins->cur_buf = NULL;
		ins->original_vf = NULL;
		pr_info("recv_common_return_frame force return the display vf: %p\n",
			ins->original_vf);
	} else {
		common_vf_put(ins, vf);
	}
	return 0;
}

static s32 recv_common_late_proc(struct video_recv_s *ins)
{
	if (!ins) {
		pr_err("%s error, empty ins\n", __func__);
		return -1;
	}
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	ins->rdma_buf = ins->cur_buf;
#endif
	return 0;
}

const struct recv_func_s recv_common_ops = {
	.early_proc = recv_common_early_process,
	.dequeue_frame = recv_common_dequeue_frame,
	.return_frame = recv_common_return_frame,
	.late_proc = recv_common_late_proc,
};

struct video_recv_s *create_video_receiver(const char *recv_name, u8 path_id)
{
	struct video_recv_s *ins = NULL;

	if (!recv_name) {
		pr_err("%s: recv_name is NULL.\n", __func__);
		goto CREATE_FAIL;
	}
	ins = kmalloc(sizeof(*ins), GFP_KERNEL);
	if (!ins)
		goto CREATE_FAIL;

	memset(ins, 0, sizeof(struct video_recv_s));
	ins->recv_name = (char *)recv_name;
	ins->vf_ops = (struct vframe_receiver_op_s *)&common_recv_func;
	ins->func = (struct recv_func_s *)&recv_common_ops;
	ins->path_id = path_id;
	spin_lock_init(&ins->lock);
	vf_receiver_init(&ins->handle,
			 ins->recv_name,
			 ins->vf_ops, ins);
	if (vf_reg_receiver(&ins->handle)) {
		pr_err("%s %s: reg recv fail\n",
		       __func__, recv_name);
		goto CREATE_FAIL;
	}
	pr_info("%s %s  %p, path_id:%d success\n",
		__func__, recv_name, ins, ins->path_id);
	return ins;
CREATE_FAIL:
	kfree(ins);
	return NULL;
}

void destroy_video_receiver(struct video_recv_s *ins)
{
	if (!ins) {
		pr_err("%s: ins is NULL.\n", __func__);
		return;
	}
	vf_unreg_receiver(&ins->handle);
	kfree(ins);
	pr_info("%s\n", __func__);
}

void switch_vf(struct video_recv_s *ins, bool switch_flag)
{
	if (!ins) {
		pr_err("%s: ins is NULL.\n", __func__);
		return;
	}
	ins->switch_vf = switch_flag;
	pr_info("set switch_flag %d\n", switch_flag);
}
