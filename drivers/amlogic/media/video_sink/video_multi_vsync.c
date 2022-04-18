// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/sched.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/video_sink/video_keeper.h>
#include "video_priv.h"
#include "video_reg.h"
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/media/registers/register.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/utils/amports_config.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include "videolog.h"
#include "video_reg.h"
#ifdef CONFIG_AM_VIDEO_LOG
#define AMLOG
#endif
#include <linux/amlogic/media/utils/amlog.h>
MODULE_AMLOG(LOG_LEVEL_ERROR, 0, LOG_DEFAULT_LEVEL_DESC, LOG_MASK_DESC);

#include <linux/amlogic/media/video_sink/vpp.h>
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#include "../common/rdma/rdma.h"
#endif
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>

#include "../common/vfm/vfm.h"
#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
#include <linux/amlogic/media/vpu_secure/vpu_secure.h>
#endif

#include <linux/math64.h>
#include "video_receiver.h"

struct video_layer_s vd_layer_vpp[2];
atomic_t video_inirq_flag_vpp[2] = {ATOMIC_INIT(0), ATOMIC_INIT(0)};
atomic_t video_unreg_flag_vpp[2] = {ATOMIC_INIT(0), ATOMIC_INIT(0)};

static int vsync_enter_line_max_vpp[2];
static int vsync_exit_line_max_vpp[2];
static bool rdma_enable_vppx_pre[2];

static unsigned int debug_flag1;
MODULE_PARM_DESC(debug_flag1, "\n debug_flag1\n");
module_param(debug_flag1, uint, 0664);

bool is_vpp0(u8 layer_id)
{
	if (vd_layer[layer_id].vpp_index == VPP0)
		return true;
	else
		return false;
}

bool is_vpp1(u8 layer_id)
{
	if (layer_id < 1)
		return false;
	if (vd_layer_vpp[layer_id - 1].vpp_index == VPP1)
		return true;
	else
		return false;
}

bool is_vpp2(u8 layer_id)
{
	if (layer_id < 1)
		return false;
	if (vd_layer_vpp[layer_id - 1].vpp_index == VPP2)
		return true;
	else
		return false;
}

int get_receiver_id(u8 layer_id)
{
	int receiver_id = VFM_PATH_VIDEO_RENDER0;

	switch (layer_id) {
	case 0:
		receiver_id = VFM_PATH_VIDEO_RENDER0;
	break;
	case 1:
		if (is_vpp0(layer_id))
			receiver_id = VFM_PATH_VIDEO_RENDER1;
		else if (is_vpp1(layer_id))
			receiver_id = VFM_PATH_VIDEO_RENDER5;
		else if (is_vpp2(layer_id))
			receiver_id = VFM_PATH_VIDEO_RENDER6;
	break;
	case 2:
		if (is_vpp0(layer_id))
			receiver_id = VFM_PATH_VIDEO_RENDER2;
		else if (is_vpp1(layer_id))
			receiver_id = VFM_PATH_VIDEO_RENDER5;
		else if (is_vpp2(layer_id))
			receiver_id = VFM_PATH_VIDEO_RENDER6;
	break;
	default:
	break;
	}
	return receiver_id;
}

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
static void vsync_rdma_vppx_process(u8 vpp_index)
{
	int ret = 0;

	if (vpp_index == VPP1)
		ret = vsync_rdma_vpp1_config();
	else if (vpp_index == VPP2)
		ret = vsync_rdma_vpp2_config();
	if (ret == 1) {
		vd_layer_vpp[0].property_changed = true;
		vd_layer_vpp[1].property_changed = true;
	}
}

static bool is_vsync_vppx_rdma_enable(u8 vpp_index)
{
	bool enable = false;

	if (vpp_index == VPP1)
		enable = is_vsync_vpp1_rdma_enable();
	else if (vpp_index == VPP2)
		enable = is_vsync_vpp2_rdma_enable();
	return enable;
}
#endif

static void pipx_render_frame(u8 vpp_index, const struct vinfo_s *vinfo)
{
	u8 vpp_id = 0;

	vpp_id = vpp_index - VPP1;
	if (vpp_id < VPP2) {
		if (vpp_index == VPP1)
			pip_render_frame(&vd_layer_vpp[vpp_id], vinfo);
		else if (vpp_index == VPP2)
			pip2_render_frame(&vd_layer_vpp[vpp_id], vinfo);
	}
}

static void pipx_swap_frame(u8 vpp_index, struct vframe_s *vf,
				  const struct vinfo_s *vinfo)
{
	u8 vpp_id = 0;

	vpp_id = vpp_index - VPP1;
	if (vpp_id < VPP2) {
		if (vpp_index == VPP1)
			pip_swap_frame(&vd_layer_vpp[vpp_id], vf, vinfo);
		else if (vpp_index == VPP2)
			pip2_swap_frame(&vd_layer_vpp[vpp_id], vf, vinfo);
	}
}
#define MAX_LOG_CNT 10

irqreturn_t vsync_isr_viux(u8 vpp_index, const struct vinfo_s *info)
{
	int hold_line;
	int enc_line;
	s32 vout_type;
	struct vframe_s *path0_new_frame = NULL;
	struct vframe_s *new_frame = NULL;
	u32 cur_blackout = 0;
	static s32 cur_vd_path_id = VFM_PATH_INVALID;
	int axis[4];
	int crop[4];
	u8 layer_id = 1, vpp_id = 0, recv_id = 0;
	s32 vd_path_id = 0;
	struct path_id_s path_id;

	path_id.vd1_path_id = vd_path_id;
	path_id.vd2_path_id = 0xffff;
	path_id.vd3_path_id = 0xffff;

	if (video_suspend && video_suspend_cycle >= 1) {
		if (log_out)
			pr_info("video suspend, vsync exit\n");
		log_out = 0;
		return IRQ_HANDLED;
	}
	if (debug_flag & DEBUG_FLAG_VSYNC_DONONE)
		return IRQ_HANDLED;

	vpp_id = vpp_index - 1;
	recv_id = vpp_id;
	layer_id = vd_layer_vpp[vpp_id].layer_id;
	vd_path_id = glayer_info[layer_id].display_path_id;

	if (cur_vd_path_id == 0xff)
		cur_vd_path_id = vd_path_id;

	vout_type = detect_vout_type(info);
	hold_line = calc_hold_line();

	glayer_info[layer_id].need_no_compress = false;
	vd_layer_vpp[vpp_id].bypass_pps = bypass_pps;
	vd_layer_vpp[vpp_id].global_debug = debug_flag;
	vd_layer_vpp[vpp_id].vout_type = vout_type;

	enc_line = get_cur_enc_line();
	if (enc_line > vsync_enter_line_max_vpp[vpp_id])
		vsync_enter_line_max_vpp[vpp_id] = enc_line;

#ifdef CONFIG_AMLOGIC_VIDEO_COMPOSER
	vsync_notify_video_composer();
#endif

	if (atomic_read(&video_unreg_flag_vpp[vpp_id]))
		goto exit;

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	if (is_vsync_vppx_rdma_enable(vpp_index)) {
		vd_layer_vpp[vpp_id].cur_canvas_id = vd_layer_vpp[vpp_id].next_canvas_id;
	} else {
		if (rdma_enable_vppx_pre[vpp_id])
			goto exit;
		vd_layer_vpp[vpp_id].cur_canvas_id = 0;
		vd_layer_vpp[vpp_id].next_canvas_id = 1;
	}
#endif
	if (gvideo_recv_vpp[recv_id])
		gvideo_recv_vpp[recv_id]->func->early_proc(gvideo_recv_vpp[recv_id], 0);

	/* video_render.5/6 toggle frame */
	if (gvideo_recv_vpp[recv_id]) {
		if (debug_flag1 & 1)
			pr_info("video_render5/6 dequeue\n");
		path0_new_frame =
			gvideo_recv_vpp[recv_id]->func->dequeue_frame(gvideo_recv_vpp[recv_id],
								      &path_id);
	}
	if (!vd_layer_vpp[vpp_id].global_output) {
		cur_vd_path_id = VFM_PATH_INVALID;
		vd_path_id = VFM_PATH_INVALID;
	}

	vd_layer_vpp[vpp_id].force_switch_mode = force_switch_vf_mode;

	if (gvideo_recv_vpp[recv_id] &&
	    vd_layer_vpp[vpp_id].dispbuf_mapping == &gvideo_recv_vpp[recv_id]->cur_buf &&
	    (gvideo_recv_vpp[recv_id]->cur_buf == &gvideo_recv_vpp[recv_id]->local_buf ||
	     !gvideo_recv_vpp[recv_id]->cur_buf) &&
	    vd_layer_vpp[vpp_id].dispbuf != gvideo_recv_vpp[recv_id]->cur_buf)
		vd_layer_vpp[vpp_id].dispbuf = gvideo_recv_vpp[recv_id]->cur_buf;

	if (vd_layer_vpp[vpp_id].switch_vf &&
	    vd_layer_vpp[vpp_id].dispbuf &&
	    vd_layer_vpp[vpp_id].dispbuf->vf_ext)
		vd_layer_vpp[vpp_id].vf_ext =
			(struct vframe_s *)vd_layer_vpp[vpp_id].dispbuf->vf_ext;
	else
		vd_layer_vpp[vpp_id].vf_ext = NULL;

	/* vd2/3 config */
	if (gvideo_recv_vpp[recv_id] &&
	    gvideo_recv_vpp[recv_id]->path_id == vd_path_id) {
		/* video_render.5/6 display on VD2/3 */
		new_frame = path0_new_frame;
		if (!new_frame) {
			if (!gvideo_recv_vpp[recv_id]->cur_buf) {
				/* video_render.5/6 no frame in display */
				if (cur_vd_path_id != vd_path_id)
					safe_switch_videolayer(layer_id, false, true);
				vd_layer_vpp[vpp_id].dispbuf = NULL;
			} else if (gvideo_recv_vpp[recv_id]->cur_buf ==
				&gvideo_recv_vpp[recv_id]->local_buf) {
				/* video_render.5/6 keep frame */
				vd_layer_vpp[vpp_id].dispbuf = gvideo_recv_vpp[recv_id]->cur_buf;
			} else if (vd_layer_vpp[vpp_id].dispbuf
				!= gvideo_recv_vpp[recv_id]->cur_buf) {
				/* video_render.5/6 has frame in display */
				new_frame = gvideo_recv_vpp[recv_id]->cur_buf;
			}
		}
		if (new_frame || gvideo_recv_vpp[recv_id]->cur_buf)
			vd_layer_vpp[vpp_id].dispbuf_mapping = &gvideo_recv_vpp[recv_id]->cur_buf;
		cur_blackout = 1;
	}

	if (!new_frame && vd_layer_vpp[vpp_id].dispbuf &&
	    is_local_vf(vd_layer_vpp[vpp_id].dispbuf)) {
		if (cur_blackout) {
			vd_layer_vpp[vpp_id].property_changed = false;
		} else if (vd_layer_vpp[vpp_id].dispbuf) {
			if (vd_layer_vpp[vpp_id].switch_vf && vd_layer_vpp[vpp_id].vf_ext)
				vd_layer_vpp[vpp_id].vf_ext->canvas0Addr =
					get_layer_display_canvas(layer_id);
			else
				vd_layer_vpp[vpp_id].dispbuf->canvas0Addr =
					get_layer_display_canvas(layer_id);
		}
	}

	if (vd_layer_vpp[vpp_id].dispbuf &&
	    (vd_layer_vpp[vpp_id].dispbuf->flag & (VFRAME_FLAG_VIDEO_COMPOSER |
		VFRAME_FLAG_VIDEO_DRM)) &&
	    !(debug_flag & DEBUG_FLAG_AXIS_NO_UPDATE)) {
		axis[0] = vd_layer_vpp[vpp_id].dispbuf->axis[0];
		axis[1] = vd_layer_vpp[vpp_id].dispbuf->axis[1];
		axis[2] = vd_layer_vpp[vpp_id].dispbuf->axis[2];
		axis[3] = vd_layer_vpp[vpp_id].dispbuf->axis[3];
		crop[0] = vd_layer_vpp[vpp_id].dispbuf->crop[0];
		crop[1] = vd_layer_vpp[vpp_id].dispbuf->crop[1];
		crop[2] = vd_layer_vpp[vpp_id].dispbuf->crop[2];
		crop[3] = vd_layer_vpp[vpp_id].dispbuf->crop[3];
		_set_video_window(&glayer_info[layer_id], axis);
		_set_video_crop(&glayer_info[layer_id], crop);
		set_alpha_scpxn(&vd_layer_vpp[vpp_id],
				vd_layer_vpp[vpp_id].dispbuf->componser_info);
		glayer_info[layer_id].zorder = vd_layer_vpp[vpp_id].dispbuf->zorder;
	}

	/* setting video display property in underflow mode */
	if (!new_frame &&
	    vd_layer_vpp[vpp_id].dispbuf &&
	    vd_layer_vpp[vpp_id].property_changed) {
		pipx_swap_frame(vpp_index, vd_layer_vpp[vpp_id].dispbuf, info);
		if (layer_id == 1)
			need_disable_vd2 = false;
		else if (layer_id == 2)
			need_disable_vd3 = false;
	} else if (new_frame) {
		pipx_swap_frame(vpp_index, new_frame, info);
		if (layer_id == 1)
			need_disable_vd2 = false;
		else if (layer_id == 2)
			need_disable_vd3 = false;
	}
	vd_layer_vpp[vpp_id].keep_frame_id = 0xff;

#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
	amvecm_on_vs
		(!is_local_vf(vd_layer_vpp[vpp_id].dispbuf)
		? vd_layer_vpp[vpp_id].dispbuf : NULL,
		new_frame,
		new_frame ? CSC_FLAG_TOGGLE_FRAME : 0,
		vd_layer_vpp[vpp_id].cur_frame_par ?
		vd_layer_vpp[vpp_id].cur_frame_par->supsc1_hori_ratio :
		0,
		vd_layer_vpp[vpp_id].cur_frame_par ?
		vd_layer_vpp[vpp_id].cur_frame_par->supsc1_vert_ratio :
		0,
		vd_layer_vpp[vpp_id].cur_frame_par ?
		vd_layer_vpp[vpp_id].cur_frame_par->spsc1_w_in :
		0,
		vd_layer_vpp[vpp_id].cur_frame_par ?
		vd_layer_vpp[vpp_id].cur_frame_par->spsc1_h_in :
		0,
		vd_layer_vpp[vpp_id].cur_frame_par ?
		vd_layer_vpp[vpp_id].cur_frame_par->cm_input_w :
		0,
		vd_layer_vpp[vpp_id].cur_frame_par ?
		vd_layer_vpp[vpp_id].cur_frame_par->cm_input_h :
		0,
		layer_id,
		vpp_index);
#endif

	if (need_disable_vd2 || need_disable_vd3)
		safe_switch_videolayer(layer_id, false, true);

	/* filter setting management */
	pipx_render_frame(vpp_index, info);
	video_secure_set(vpp_index);

	if (vd_layer_vpp[vpp_id].dispbuf &&
	    (vd_layer_vpp[vpp_id].dispbuf->flag & VFRAME_FLAG_FAKE_FRAME))
		safe_switch_videolayer(layer_id, false, true);

	/* all frames has been renderred, so reset new frame flag */
	vd_layer_vpp[vpp_id].new_frame = false;
exit:
	vppx_blend_update(info, vpp_index);
	if (gvideo_recv_vpp[recv_id])
		gvideo_recv_vpp[recv_id]->func->late_proc(gvideo_recv_vpp[recv_id]);

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	vsync_rdma_vppx_process(vpp_index);
	rdma_enable_vppx_pre[vpp_id] = is_vsync_vppx_rdma_enable(vpp_index);
#endif

	enc_line = get_cur_enc_line();
	if (enc_line > vsync_exit_line_max_vpp[vpp_id])
		vsync_exit_line_max_vpp[vpp_id] = enc_line;
	if (video_suspend)
		video_suspend_cycle++;
	vpu_work_process();
	cur_vd_path_id = vd_path_id;

	return IRQ_HANDLED;
}

irqreturn_t vsync_isr_viu2(int irq, void *dev_id)
{
	irqreturn_t ret;
	const struct vinfo_s *info = NULL;

	atomic_set(&video_inirq_flag_vpp[0], 1);
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	info = get_current_vinfo2();
#endif
	ret = vsync_isr_viux(VPP1, info);
	atomic_set(&video_inirq_flag_vpp[0], 0);
	return ret;
}

irqreturn_t vsync_isr_viu3(int irq, void *dev_id)
{
	irqreturn_t ret;
	const struct vinfo_s *info = NULL;

	atomic_set(&video_inirq_flag_vpp[1], 1);
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	info = get_current_vinfo3();
#endif
	ret = vsync_isr_viux(VPP2, info);
	atomic_set(&video_inirq_flag_vpp[1], 0);
	return ret;
}

int is_in_vsync_isr_viu2(void)
{
	if (atomic_read(&video_inirq_flag_vpp[0]) > 0)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(is_in_vsync_isr_viu2);

int is_in_vsync_isr_viu3(void)
{
	if (atomic_read(&video_inirq_flag_vpp[1]) > 0)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(is_in_vsync_isr_viu3);

